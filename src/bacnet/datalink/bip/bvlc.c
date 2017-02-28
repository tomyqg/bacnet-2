/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bvlc.c
 * Original Author:  linzhixian, 2014-10-10
 *
 * BACNET虚拟链路层协议
 *
 * History
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "bacnet/bacdef.h"
#include "bacnet/bip.h"
#include "bip_def.h"
#include "bacnet/bacint.h"

static int bvlc_send_mpdu(datalink_bip_t *bip, struct sockaddr_in *dst, uint8_t *mpdu, 
            uint16_t mpdu_len)
{
    struct sockaddr_in bvlc_dst = {0};
    int sock_fd;
    int rv;
    
    if (mpdu == NULL) {
        BIP_ERROR("%s: invalid mpdu\r\n", __func__);
        return -EINVAL;
    }

    if ((!bip) || (mpdu_len < BVLC_HDR_LEN) || (dst == NULL)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    sock_fd = bip->sock_uip;
    if (sock_fd < 0) {
        BIP_ERROR("%s: invalid port(%d) sock_fd(%d)\r\n", __func__, bip->dl.port_id, sock_fd);
        return -EPERM;
    }

    bvlc_dst.sin_family = AF_INET;
    bvlc_dst.sin_addr.s_addr = dst->sin_addr.s_addr;
    bvlc_dst.sin_port = dst->sin_port;
    memset(&(bvlc_dst.sin_zero), '\0', sizeof(bvlc_dst.sin_zero));

    rv = sendto(sock_fd, mpdu, mpdu_len, MSG_DONTWAIT, (struct sockaddr *)&bvlc_dst, sizeof(struct sockaddr));
    if (rv < 0) {
        BIP_ERROR("%s: sendto failed cause %s\r\n", __func__, strerror(errno));
    } else if (rv != mpdu_len) {
        BIP_ERROR("%s: sockfd(%d) send failed cause only send %d bytes in %d bytes\r\n", __func__, 
            sock_fd, rv, mpdu_len);
        rv = -EPERM;
    } else {
        rv = OK;
    }

    return rv;
}

int bvlc_bdt_forward_npdu(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *npdu)
{
    bdt_entry_t *bdt;
    struct sockaddr_in dst_bip;
    uint8_t *mpdu;
    uint16_t mpdu_len;
    int bdt_size;
    int i;
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (npdu == NULL) || (npdu->data == NULL) 
            || (npdu->data_len == 0)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    rv = bacnet_buf_push(npdu, FORWARDED_NPDU_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    mpdu_len = npdu->data_len;
    mpdu = npdu->data;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_FORWARDED_NPDU;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
    memcpy(&mpdu[4], &(src->sin_addr.s_addr), IP_ADDRESS_LEN);
    memcpy(&mpdu[8], &(src->sin_port), UDP_PORT_LEN);

    RWLOCK_RDLOCK(&(bip->bbmd->bdt_lock));

    bdt = bip->bbmd->bdt;
    bdt_size = bip->bbmd->bdt_size;

    for (i = 0; i < bdt_size; i++) {
        if (i == bip->bbmd->local_entry)
            continue;

        dst_bip.sin_addr.s_addr = ((~(bdt[i].bcast_mask.s_addr)) | (bdt[i].dst_addr.s_addr));
        dst_bip.sin_port = bdt[i].dst_port;

        rv = bvlc_send_mpdu(bip, &dst_bip, mpdu, mpdu_len);
        if (rv < 0) {
            BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
        }

        BIP_VERBOS("%s: %s:%04X\r\n", __func__, inet_ntoa(dst_bip.sin_addr), 
            ntohs(dst_bip.sin_port));
    }

    RWLOCK_UNLOCK(&(bip->bbmd->bdt_lock));

    (void)bacnet_buf_pull(npdu, FORWARDED_NPDU_HDR_LEN);
    
    return OK;
}

int bvlc_fdt_forward_npdu(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *npdu)
{
    fdt_entry_t *entry;
    struct sockaddr_in dst_bip;
    uint8_t *mpdu;
    uint16_t mpdu_len;
    int i;
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (npdu == NULL) || (npdu->data == NULL) 
            || (npdu->data_len == 0)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    rv = bacnet_buf_push(npdu, FORWARDED_NPDU_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    mpdu_len = npdu->data_len;
    mpdu = npdu->data;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_FORWARDED_NPDU;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
    memcpy(&mpdu[4], &(src->sin_addr.s_addr), IP_ADDRESS_LEN);
    memcpy(&mpdu[8], &(src->sin_port), UDP_PORT_LEN);

    RWLOCK_RDLOCK(&(bip->bbmd->fdt_lock));

    hash_for_each(bip->bbmd->fdt, i, entry, hnode) {
        dst_bip.sin_addr.s_addr = entry->dst_addr.s_addr;
        dst_bip.sin_port = entry->dst_port;

        /* don't send to src ip address and same port */
        if ((dst_bip.sin_addr.s_addr == src->sin_addr.s_addr)
                && (dst_bip.sin_port == src->sin_port)) {
            continue;
        }

        rv = bvlc_send_mpdu(bip, &dst_bip, mpdu, mpdu_len);
        if (rv < 0) {
            BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
        }

        BIP_VERBOS("%s: %s:%04X\r\n", __func__, inet_ntoa(dst_bip.sin_addr),
            ntohs(dst_bip.sin_port));
    }

    RWLOCK_UNLOCK(&(bip->bbmd->fdt_lock));

    (void)bacnet_buf_pull(npdu, FORWARDED_NPDU_HDR_LEN);

    return OK;
}

/* send bvlc result message */
static int bvlc_send_bvlc_result(datalink_bip_t *bip, struct sockaddr_in *dst, 
            BACNET_BVLC_RESULT result_code)
{
    uint8_t mpdu[6];
    int rv;
    
    if ((bip == NULL) || (dst == NULL)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_RESULT;
    (void)encode_unsigned16(&mpdu[2], (uint16_t)6);
    (void)encode_unsigned16(&mpdu[4], (uint16_t)result_code);

    rv = bvlc_send_mpdu(bip, dst, mpdu, 6);
    if (rv < 0) {
        BIP_ERROR("%s: send result_code(%d) failed(%d)\r\n", __func__, result_code, rv);
    }
    
    return rv;
}

int bvlc_send_read_bdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd)
{
    uint8_t mpdu[BVLC_HDR_LEN];
    uint16_t mpdu_len;
    int rv;
    
    if ((bip == NULL) || (dst_bbmd == NULL)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    mpdu_len = BVLC_HDR_LEN;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_READ_BROADCAST_DISTRIBUTION_TABLE;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);

    rv = bvlc_send_mpdu(bip, dst_bbmd, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int bvlc_send_write_bdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd, bdt_entry_t *bdt,
        size_t len)
{
    uint8_t mpdu[BIP_MAX_DATA_LEN + BVLC_HDR_LEN];
    uint16_t mpdu_len;
    int rv;
    int i;

    if ((bip == NULL) || (dst_bbmd == NULL) || (bdt == NULL)) {
        BIP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if ((len < 1) || (len > BDT_MAX_SIZE)) {
        BIP_ERROR("%s: invalid bdt size(%d)\r\n", __func__, len);
        return -EINVAL;
    }

    mpdu_len = BVLC_HDR_LEN;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE;
    (void)encode_unsigned16(&mpdu[2], len * BBMD_TABLE_ENTRY_SIZE + BVLC_HDR_LEN);

    for (i = 0; i < len; ++i) {
        memcpy(&mpdu[mpdu_len], &bdt[i].dst_addr, IP_ADDRESS_LEN);
        mpdu_len += IP_ADDRESS_LEN;
        memcpy(&mpdu[mpdu_len], &bdt[i].dst_port, UDP_PORT_LEN);
        mpdu_len += UDP_PORT_LEN;
        memcpy(&mpdu[mpdu_len], &bdt[i].bcast_mask, BCAST_MASK_LEN);
        mpdu_len += BCAST_MASK_LEN;
    }

    rv = bvlc_send_mpdu(bip, dst_bbmd, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

static int bvlc_send_read_bdt_ack(datalink_bip_t *bip, struct sockaddr_in *dst)
{
    bdt_entry_t *bdt;
    uint16_t mpdu_len;
    int bdt_size;
    int offset;
    int i;
    int rv;
    
    if ((bip == NULL) || (dst == NULL)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    RWLOCK_RDLOCK(&(bip->bbmd->bdt_lock));

    bdt = bip->bbmd->bdt;
    bdt_size = bip->bbmd->bdt_size;
    
    mpdu_len = BVLC_HDR_LEN + (BBMD_TABLE_ENTRY_SIZE * bdt_size);
    uint8_t mpdu[mpdu_len];
    
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_READ_BROADCAST_DISTRIBUTION_TABLE_ACK;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
   
    offset = BVLC_DATA_OFFSET;
    for (i = 0; i < bdt_size; i++) {
        memcpy(&mpdu[offset], &(bdt[i].dst_addr.s_addr), IP_ADDRESS_LEN);
        offset += IP_ADDRESS_LEN;
        memcpy(&mpdu[offset], &(bdt[i].dst_port), UDP_PORT_LEN);
        offset += UDP_PORT_LEN;
        memcpy(&mpdu[offset], &(bdt[i].bcast_mask), BCAST_MASK_LEN);
        offset += BCAST_MASK_LEN;
    }

    RWLOCK_UNLOCK(&(bip->bbmd->bdt_lock));
    
    rv = bvlc_send_mpdu(bip, dst, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int bvlc_send_read_fdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd)
{
    uint8_t mpdu[BVLC_HDR_LEN];
    uint16_t mpdu_len;
    int rv;

    if ((dst_bbmd == NULL) || (!bip)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    mpdu_len = BVLC_HDR_LEN;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_READ_FOREIGN_DEVICE_TABLE;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);

    rv = bvlc_send_mpdu(bip, dst_bbmd, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

static int bvlc_send_read_fdt_ack(datalink_bip_t *bip, struct sockaddr_in *dst)
{
    fdt_entry_t *entry;
    uint16_t mpdu_len;
    int offset;
    int i;
    int rv;
    
    if ((dst == NULL) || (!bip)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    RWLOCK_RDLOCK(&(bip->bbmd->fdt_lock));
    
    mpdu_len = BVLC_HDR_LEN + (FD_TABLE_ENTRY_SIZE * bip->bbmd->fdt_size);
    uint8_t mpdu[mpdu_len];
    
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_READ_FOREIGN_DEVICE_TABLE_ACK;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
    
    offset = BVLC_DATA_OFFSET;
    unsigned now = el_current_millisecond();
    hash_for_each(bip->bbmd->fdt, i, entry, hnode) {
        unsigned remaining = el_timer_expire(entry->timer) - now;
        if (remaining > 65535*1000)
            remaining = 0;

        memcpy(&mpdu[offset], &(entry->dst_addr.s_addr), IP_ADDRESS_LEN);
        offset += IP_ADDRESS_LEN;
        memcpy(&mpdu[offset], &(entry->dst_port), UDP_PORT_LEN);
        offset += UDP_PORT_LEN;
        offset += encode_unsigned16(&mpdu[offset], entry->time_to_live);
        offset += encode_unsigned16(&mpdu[offset], remaining/1000);
    }

    RWLOCK_UNLOCK(&(bip->bbmd->fdt_lock));
    
    rv = bvlc_send_mpdu(bip, dst, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

/* receive bvlc result message */
int bvlc_receive_bvlc_result(datalink_bip_t *bip, bacnet_buf_t *mpdu)
{
    uint16_t result_code;
    
    if ((bip == NULL) || (mpdu == NULL) || (mpdu->data == NULL) || (mpdu->data_len != 6)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    (void)decode_unsigned16(&(mpdu->data[BVLC_DATA_OFFSET]), &result_code);
    switch (result_code) {
    case BVLC_RESULT_SUCCESSFUL_COMPLETION:
        BIP_VERBOS("%s: Successful completion\r\n", __func__);
        break;

    case BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK:
        BIP_ERROR("%s: Write-Broadcast-Distribution-Table NAK\r\n", __func__);
        break;

    case BVLC_RESULT_READ_BROADCAST_DISTRIBUTION_TABLE_NAK:
        BIP_ERROR("%s: Read-Broadcast-Distribution-Table NAK\r\n", __func__);
        break;

    case BVLC_RESULT_REGISTER_FOREIGN_DEVICE_NAK:
        BIP_ERROR("%s: Register-Foreign-Device NAK\r\n", __func__);
        break;

    case BVLC_RESULT_READ_FOREIGN_DEVICE_TABLE_NAK:
        BIP_ERROR("%s: Read-Foreign-Device-Table NAK\r\n", __func__);
        break;
        
    case BVLC_RESULT_DELETE_FOREIGN_DEVICE_TABLE_ENTRY_NAK:
        BIP_ERROR("%s: Delete-Foreign-Device-Table-Entry NAK\r\n", __func__);
        break;

    case BVLC_RESULT_DISTRIBUTE_BROADCAST_TO_NETWORK_NAK:
        BIP_ERROR("%s: Distribute-Broadcast-To-Network NAK\r\n", __func__);
        break;

    default:
        BIP_ERROR("%s: Unknown BVLC-Result Code(0x%2x)\r\n", __func__, result_code);
        break;
    }

    return OK;
}

int bvlc_receive_write_bdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu)
{
    bdt_entry_t *bdt;
    uint8_t *data;
    int bdt_size;
    int data_len;
    int data_offset;
    int i;
    int local_idx;
    
    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len == 0)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    BIP_VERBOS("%s: Receive Write-BDT\r\n", __func__);
    
    data_len = mpdu->data_len - BVLC_HDR_LEN;
    if ((data_len == 0) || (data_len % BBMD_TABLE_ENTRY_SIZE != 0)) {
        BIP_ERROR("%s: invalid data_len(%d)\r\n", __func__, data_len);
        (void)bvlc_send_bvlc_result(bip, src, BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK);
        return -EINVAL;
    }

    bdt_size = data_len / BBMD_TABLE_ENTRY_SIZE;
    bdt = (bdt_entry_t *)malloc(sizeof(bdt_entry_t) * bdt_size);
    if (bdt == NULL) {
        BIP_ERROR("%s: malloc bdt failed\r\n", __func__);
        (void)bvlc_send_bvlc_result(bip, src, BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK);
        return -ENOMEM;
    }

    local_idx = -1;
    data_offset = 0;
    data = (uint8_t *)&(mpdu->data[BVLC_DATA_OFFSET]);
    for (i = 0; i < bdt_size; i++) {
        memcpy(&(bdt[i].dst_addr.s_addr), &data[data_offset], IP_ADDRESS_LEN);
        data_offset += IP_ADDRESS_LEN;
        memcpy(&(bdt[i].dst_port), &data[data_offset], UDP_PORT_LEN);
        data_offset += UDP_PORT_LEN;
        memcpy(&(bdt[i].bcast_mask), &data[data_offset], BCAST_MASK_LEN);
        data_offset += BCAST_MASK_LEN;

        if (!check_broadcast_mask(bdt[i].bcast_mask)) {
            BIP_ERROR("%s: invalid netmask %s\r\n", __func__, inet_ntoa(bdt[i].bcast_mask));
            goto err;
        }

        if ((bdt[i].dst_addr.s_addr == bip->sin.sin_addr.s_addr)
                && (bdt[i].dst_port == bip->sin.sin_port)) {
            if (local_idx >= 0) {
                BIP_ERROR("%s: duplicated local entry\r\n", __func__);
                goto err;
            }
            
            if ((bdt[i].bcast_mask.s_addr != -1)
                    && (bdt[i].bcast_mask.s_addr != bip->netmask.s_addr)) {
                BIP_ERROR("%s: invalid local netmask %s\r\n", __func__, inet_ntoa(bdt[i].bcast_mask));
                goto err;
            }
            local_idx = i;
        }
    }

    if (local_idx < 0) {
        BIP_ERROR("%s: no local entry\r\n", __func__);
        goto err;
    }

    RWLOCK_WRLOCK(&(bip->bbmd->bdt_lock));

    if (bip->bbmd->bdt) {
        free(bip->bbmd->bdt);
    }

    bip->bbmd->bdt = bdt;
    bip->bbmd->bdt_size = bdt_size;
    bip->bbmd->local_entry = local_idx;

    RWLOCK_UNLOCK(&(bip->bbmd->bdt_lock));

    (void)bvlc_send_bvlc_result(bip, src, BVLC_RESULT_SUCCESSFUL_COMPLETION);

    bdt_push_start(bip);

    return OK;

err:
    free(bdt);
    (void)bvlc_send_bvlc_result(bip, src, BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK);
    
    return -EPERM;
}

int bvlc_receive_read_bdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu)
{
    int rv;

    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len != BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    BIP_VERBOS("%s: Receive Read-BDT\r\n", __func__);

    rv = bvlc_send_read_bdt_ack(bip, src);
    if (rv < 0) {
        BIP_ERROR("%s: send Read-BDT-ACK failed\r\n", __func__);
        rv = bvlc_send_bvlc_result(bip, src, BVLC_RESULT_READ_BROADCAST_DISTRIBUTION_TABLE_NAK);
        if (rv < 0) {
            BIP_ERROR("%s: send Read-BDT-NAK failed\r\n", __func__);
        }
    }
    
    return rv;
}

int bvlc_receive_read_bdt_ack(datalink_bip_t *bip, bacnet_buf_t *mpdu)
{
    struct in_addr addr;
    struct in_addr mask;
    uint8_t *data;
    uint16_t port;
    uint16_t data_len;
    uint16_t entries;
    int offset;
    int i;

    if ((bip == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len <= BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    BIP_VERBOS("%s: Receive Read-BDT-ACK\r\n", __func__);

    data_len = mpdu->data_len - BVLC_HDR_LEN;
    if (data_len % BBMD_TABLE_ENTRY_SIZE != 0) {
        BIP_ERROR("%s: invalid data_len(%d)\r\n", __func__, data_len);
        return -EINVAL;
    }
    
    entries = data_len / BBMD_TABLE_ENTRY_SIZE;
    printf("The Number of BBMD_BDT Entry: %d\r\n", entries);
    printf("[IP_ADDRESS]\t\t[PORT]\t\t[MASK]\r\n");

    offset = 0;
    data = &(mpdu->data[BVLC_DATA_OFFSET]);
    for (i = 0; i < entries; i++) {
        memcpy(&addr, &data[offset], IP_ADDRESS_LEN);
        offset += IP_ADDRESS_LEN;
        offset += decode_unsigned16(&data[offset], &port);
        memcpy(&mask, &data[offset], BCAST_MASK_LEN);
        offset += BCAST_MASK_LEN;
        
        printf("%s\t\t", inet_ntoa(addr));
        printf("%d\t\t", port);
        printf("%s\r\n", inet_ntoa(mask));
    }
    printf("\r\n");
    
    return OK;
}

/* BBMD设备转发forwarded_npdu */
int bvlc_receive_forwarded_npdu(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu)
{
    struct sockaddr_in dst_bip;
    int rv;

    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len <= FORWARDED_NPDU_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    /* route not support broadcast, send to BBMD's subnet using the B/IP broadcast address */
    if (bip->bbmd->bdt[bip->bbmd->local_entry].bcast_mask.s_addr == -1) {
        dst_bip.sin_addr.s_addr = bip->bcast_sin.sin_addr.s_addr;
        dst_bip.sin_port = bip->bcast_sin.sin_port;

        rv = bvlc_send_mpdu(bip, &dst_bip, mpdu->data, mpdu->data_len);
        if (rv < 0) {
            BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
        }
    }

    rv = bacnet_buf_pull(mpdu, FORWARDED_NPDU_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = bvlc_fdt_forward_npdu(bip, src, mpdu);
    if (rv < 0) {
        BIP_ERROR("%s: fdt forward npdu failed(%d)\r\n", __func__, rv);
    }

    (void)bacnet_buf_push(mpdu, FORWARDED_NPDU_HDR_LEN);
    
    return rv;   
}

/* BBMD设备的外部设备表项老化 */
static void fdt_aging(el_timer_t *timer)
{
    fdt_entry_t *entry = (fdt_entry_t*)timer->data;
    bbmd_data_t *bbmd = entry->bbmd;

    BIP_VERBOS("%s: foreign device %s:%04X timeout(%d seconds)\r\n", __func__,
        inet_ntoa(entry->dst_addr),
        ntohs(entry->dst_port),
        entry->time_to_live);

    RWLOCK_WRLOCK(&(bbmd->fdt_lock));

    hash_del(&entry->hnode);
    free(entry);

    if (bbmd->fdt_size)
        bbmd->fdt_size--;

    RWLOCK_UNLOCK(&(bbmd->fdt_lock));
}

int bvlc_receive_register_foreign_device(datalink_bip_t *bip, struct sockaddr_in *src, 
        bacnet_buf_t *mpdu)
{
    fdt_entry_t *entry;
    BACNET_BVLC_RESULT result_code;
    uint16_t time_to_live;
    bool status;
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len != 6)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    BIP_VERBOS("%s: Receive Register-Foreign-Device\r\n", __func__);

    (void)decode_unsigned16(&(mpdu->data[BVLC_DATA_OFFSET]), &time_to_live);

    status = false;

    RWLOCK_WRLOCK(&(bip->bbmd->fdt_lock));

    unsigned key = (ROTATE_LEFT(src->sin_addr.s_addr, 16)) + src->sin_port;

    hash_for_each_possible(bip->bbmd->fdt, entry, hnode, key) {
        if (entry->dst_addr.s_addr == src->sin_addr.s_addr
                && entry->dst_port == src->sin_port) {
            entry->time_to_live = time_to_live;
            el_timer_mod(&el_default_loop, entry->timer, time_to_live*1000);
            status = true;
            goto out;
        }
    }

    if (bip->bbmd->fdt_size >= FDT_MAX_SIZE) {
        BIP_ERROR("%s: FTD is already full\r\n", __func__);
        status = false;
        goto out;
    }

    entry = (fdt_entry_t*)malloc(sizeof(fdt_entry_t));
    if (!entry) {
        BIP_ERROR("%s: not enough memory\r\n", __func__);
        status = false;
        goto out;
    }
    memset(entry, 0, sizeof(fdt_entry_t));

    entry->dst_addr = src->sin_addr;
    entry->dst_port = src->sin_port;
    entry->bbmd = bip->bbmd;
    entry->time_to_live = time_to_live;
    entry->timer = el_timer_create(&el_default_loop, time_to_live*1000);
    if (!entry->timer) {
        BIP_ERROR("%s: create timer failed\r\n", __func__);
        free(entry);
        status = false;
        goto out;
    }
    entry->timer->data = entry;
    entry->timer->handler = fdt_aging;

    hash_add(bip->bbmd->fdt, &entry->hnode, key);
    bip->bbmd->fdt_size++;

    status = true;

out:
    RWLOCK_UNLOCK(&(bip->bbmd->fdt_lock));

    if (status) {
        result_code = BVLC_RESULT_SUCCESSFUL_COMPLETION;
    } else {
        result_code = BVLC_RESULT_REGISTER_FOREIGN_DEVICE_NAK;
    }

    rv = bvlc_send_bvlc_result(bip, src, result_code);
    if (rv < 0) {
        BIP_ERROR("%s: send bvlc result failed(%d)\r\n", __func__, rv);
    }
    
    return rv;
}

int bvlc_receive_read_fdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu)
{
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len != BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    BIP_VERBOS("%s: Receive Read-FDT\r\n", __func__);

    rv = bvlc_send_read_fdt_ack(bip, src);
    if (rv < 0) {
        BIP_ERROR("%s: send Read-FDT-ACK failed\r\n", __func__);
        
        rv = bvlc_send_bvlc_result(bip, src, BVLC_RESULT_READ_FOREIGN_DEVICE_TABLE_NAK);
        if (rv < 0) {
            BIP_ERROR("%s: send Read-FDT-NAK failed\r\n", __func__);
        }
    }
    
    return rv;
}

int bvlc_receive_read_fdt_ack(datalink_bip_t *bip, bacnet_buf_t *mpdu)
{
    struct in_addr addr;
    uint8_t *data;
    uint16_t port;
    uint16_t data_len;
    uint16_t entries;
    uint16_t time_to_live;
    uint16_t seconds_remaining;
    int offset;
    int i;

    if ((bip == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len <= BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid mpdu\r\n", __func__);
        return -EINVAL;
    }

    BIP_VERBOS("%s: Receive Read-FDT-ACK\r\n", __func__);

    data_len = mpdu->data_len - BVLC_HDR_LEN;
    if (data_len % FD_TABLE_ENTRY_SIZE != 0) {
        BIP_ERROR("%s: invalid data_len(%d)\r\n", __func__, data_len);
        return -EINVAL;
    }
    
    entries = data_len / FD_TABLE_ENTRY_SIZE;
    printf("The Number of BBMD_FDT Entry: %d\r\n", entries);
    printf("[IP ADDRESS]\t\t[PORT]\t\t[Time-to-Live]\t\t[Seconds_remaining]\r\n");

    offset = 0;
    data = &(mpdu->data[BVLC_DATA_OFFSET]);
    for (i = 0; i < entries; i++) {
        memcpy(&addr, &data[offset], IP_ADDRESS_LEN);
        offset += IP_ADDRESS_LEN;
        offset += decode_unsigned16(&data[offset], &port);
        offset += decode_unsigned16(&data[offset], &time_to_live);
        offset += decode_unsigned16(&data[offset], &seconds_remaining);
        
        printf("%s\t\t", inet_ntoa(addr));
        printf("%d\t\t", port);
        printf("%d\t\t", time_to_live);
        printf("\t%d\r\n", seconds_remaining);
    }
    printf("\r\n");
    
    return OK;
}

int bvlc_receive_delete_fdt_entry(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu)
{
    fdt_entry_t *entry;
    BACNET_BVLC_RESULT result_code;
    struct in_addr sin;
    uint16_t port;
    bool status;
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) || 
            (mpdu->data_len != BVLC_DEL_FDT_ENTRY_SIZE)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    BIP_VERBOS("%s: Receive Delete-FDT-Entry\r\n", __func__);
    
    memcpy(&sin, &(mpdu->data[BVLC_DATA_OFFSET]), IP_ADDRESS_LEN);
    memcpy(&port, &(mpdu->data[BVLC_DATA_OFFSET + IP_ADDRESS_LEN]), UDP_PORT_LEN);

    status = false;

    RWLOCK_WRLOCK(&(bip->bbmd->fdt_lock));

    unsigned key = (ROTATE_LEFT(sin.s_addr, 16)) + port;

    hash_for_each_possible(bip->bbmd->fdt, entry, hnode, key) {
        if (entry->dst_addr.s_addr == sin.s_addr
                && entry->dst_port == port) {
            el_timer_destroy(&el_default_loop, entry->timer);
            hash_del(&entry->hnode);
            free(entry);
            if (bip->bbmd->fdt_size)
                bip->bbmd->fdt_size--;
            status = true;
            break;
        }
    }

    RWLOCK_UNLOCK(&(bip->bbmd->fdt_lock));

    if (status) {
        result_code = BVLC_RESULT_SUCCESSFUL_COMPLETION;
    } else {
        result_code = BVLC_RESULT_DELETE_FOREIGN_DEVICE_TABLE_ENTRY_NAK;
    }

    rv = bvlc_send_bvlc_result(bip, src, result_code);
    if (rv < 0) {
        BIP_ERROR("%s: send BVLC-Result(0x%x) failed(%d)\r\n", __func__, 
            result_code, rv);
    }

    return rv;
}

/* BBMD设备转发distribute_broadcast_to_network报文 */
int bvlc_receive_distribute_bcast_to_network(datalink_bip_t *bip, struct sockaddr_in *src,
        bacnet_buf_t *mpdu)
{
    struct sockaddr_in dst_bip;
    uint8_t *pdu;
    uint16_t pdu_len;
    int rv;

    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len <= BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    rv = bacnet_buf_push(mpdu, BIP_ADDRESS_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        goto out1;
    }

    pdu = mpdu->data;
    pdu_len = mpdu->data_len;
    pdu[0] = BVLL_TYPE_BACNET_IP;
    pdu[1] = BVLC_FORWARDED_NPDU;
    (void)encode_unsigned16(&pdu[2], pdu_len);
    memcpy(&pdu[4], &(src->sin_addr.s_addr), IP_ADDRESS_LEN);
    memcpy(&pdu[8], &(src->sin_port), UDP_PORT_LEN);

    dst_bip.sin_addr.s_addr = bip->bcast_sin.sin_addr.s_addr;
    dst_bip.sin_port = bip->bcast_sin.sin_port;
    
    rv = bvlc_send_mpdu(bip, &dst_bip, pdu, pdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send failed(%d)\r\n", __func__, rv);
        goto out2;
    }

    rv = bacnet_buf_pull(mpdu, FORWARDED_NPDU_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
        goto out2;
    }

    rv = bvlc_bdt_forward_npdu(bip, src, mpdu);
    if (rv < 0) {
        BIP_ERROR("%s: bdt forward npdu failed(%d)\r\n", __func__, rv);
        goto out3;
    }

    rv = bvlc_fdt_forward_npdu(bip, src, mpdu);
    if (rv < 0) {
        BIP_ERROR("%s: fdt forward npdu failed(%d)\r\n", __func__, rv);
        goto out3;
    }

out3:
    (void)bacnet_buf_push(mpdu, FORWARDED_NPDU_HDR_LEN);

out2:
    (void)bacnet_buf_pull(mpdu, BIP_ADDRESS_LEN);
    
out1:
    if (rv < 0) {
        rv = bvlc_send_bvlc_result(bip, src, BVLC_RESULT_DISTRIBUTE_BROADCAST_TO_NETWORK_NAK);
        if (rv < 0) {
            BIP_ERROR("%s: send BVLC-Result(0x0060) failed(%d)\r\n", __func__, 
                rv);
        }
    }

    return rv;
}

/* BBMD设备转发original_broadcast_npdu */
int bvlc_receive_original_broadcast_npdu(datalink_bip_t *bip, struct sockaddr_in *src, 
        bacnet_buf_t *mpdu)
{
    int rv;
    
    if ((bip == NULL) || (src == NULL) || (mpdu == NULL) || (mpdu->data == NULL) 
            || (mpdu->data_len <= BVLC_HDR_LEN)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!bip->bbmd) {
        BIP_ERROR("%s: Unexpected Msg for Non-BBMD\r\n", __func__);
        return -EPERM;
    }

    rv = bacnet_buf_pull(mpdu, BVLC_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = bvlc_bdt_forward_npdu(bip, src, mpdu);
    if (rv < 0) {
        BIP_ERROR("%s: bdt forward npdu failed(%d)\r\n", __func__, rv);
    }

    rv = bvlc_fdt_forward_npdu(bip, src, mpdu);
    if (rv < 0) {
        BIP_ERROR("%s: fdt forward npdu failed(%d)\r\n", __func__, rv);
    }

    (void)bacnet_buf_push(mpdu, BVLC_HDR_LEN);
    
    return rv;
}

/* Register as a foreign device with the indicated BBMD */
int bvlc_register_with_bbmd(datalink_bip_t *bip, struct sockaddr_in *remote_bbmd,
        uint16_t ttl_seconds)
{
    uint16_t mpdu_len;
    int rv;

    if ((remote_bbmd == NULL) || (!bip)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!(bip->fd_client)) {
        BIP_ERROR("%s: Unexpected Msg for Non-Foreign Device\r\n", __func__);
        return -EPERM;
    }

    mpdu_len = 6;
    uint8_t mpdu[mpdu_len];
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = BVLC_REGISTER_FOREIGN_DEVICE;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
    (void)encode_unsigned16(&mpdu[4], ttl_seconds);
    
    rv = bvlc_send_mpdu(bip, remote_bbmd, mpdu, mpdu_len);
    if (rv < 0) {
        BIP_ERROR("%s: send Register-Foreign-Device Msg failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

bbmd_data_t *bbmd_create(void)
{
    bbmd_data_t *bbmd;
    int rv;
    
    bbmd = (bbmd_data_t *)malloc(sizeof(bbmd_data_t));
    if (!bbmd) {
        BIP_ERROR("%s: no enough memory\r\n", __func__);
        return NULL;
    }
    memset(bbmd, 0, sizeof(bbmd_data_t));

    rv = pthread_rwlock_init(&(bbmd->fdt_lock), NULL);
    if (rv) {
        BIP_ERROR("%s: init fdt rwlock failed(%d)\r\n", __func__, rv);
        free(bbmd);
        return NULL;
    }

    rv = pthread_rwlock_init(&(bbmd->bdt_lock), NULL);
    if (rv) {
        BIP_ERROR("%s: init bdt rwlock failed(%d)\r\n", __func__, rv);
        free(bbmd);
        return NULL;
    }

    return bbmd;
}

void bbmd_destroy(bbmd_data_t *bbmd)
{
    struct hlist_node *tmp;
    fdt_entry_t *entry;
    int i;
    
    if (bbmd == NULL) {
        BIP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    el_sync(&el_default_loop);
    
    hash_for_each_safe(bbmd->fdt, i, entry, tmp, hnode) {
        el_timer_destroy(&el_default_loop, entry->timer);
        free(entry);
    }

    if (bbmd->bdt) {
        free(bbmd->bdt);
    }

    if (bbmd->push.timer) {
        el_timer_destroy(&el_default_loop, bbmd->push.timer);
    }

    free(bbmd);
    el_unsync(&el_default_loop);
}

/* 定时更新远程BBMD设备的BDT表 */
static void bdt_push(el_timer_t *timer)
{
    struct sockaddr_in remote_bbmd;
    datalink_bip_t *bip;
    bbmd_data_t *bbmd;;
    int rv;

    bip = (datalink_bip_t *)timer->data;
    bbmd = bip->bbmd;
    
    RWLOCK_WRLOCK(&(bbmd->fdt_lock));

    if (bbmd->push.idx == bbmd->local_entry) {
        if (++bbmd->push.idx >= bbmd->bdt_size) {
            bbmd->push.idx = 0;
        }
    }

    remote_bbmd.sin_addr.s_addr = bbmd->bdt[bbmd->push.idx].dst_addr.s_addr;
    remote_bbmd.sin_port = bbmd->bdt[bbmd->push.idx].dst_port;

    rv = bvlc_send_write_bdt(bip, &remote_bbmd, bbmd->bdt, bbmd->bdt_size);
    BIP_VERBOS("%s: push bdt to %s:%04X\r\n", __func__, inet_ntoa(remote_bbmd.sin_addr),
        ntohs(remote_bbmd.sin_port));
    if (rv < 0) {
        BIP_ERROR("%s: send write bdt failed(%d)\r\n", __func__, rv);
    } else {
        if (++bbmd->push.idx >= bbmd->bdt_size) {
            bbmd->push.idx = 0;
        }
    }

    RWLOCK_UNLOCK(&(bbmd->fdt_lock));

    el_timer_mod(&el_default_loop, timer, bbmd->push.each_ms);
}

void bdt_push_start(datalink_bip_t *bip)
{
    if (bip == NULL) {
        BIP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    if (bip->bbmd == NULL) {
        BIP_WARN("%s: bbmd not enable\r\n", __func__);
        return;
    }

    if (bip->bbmd->push.interval == 0) {
        BIP_WARN("%s: push not enable\r\n", __func__);
        return;
    }

    RWLOCK_RDLOCK(&(bip->bbmd->bdt_lock));

    if (bip->bbmd->bdt_size <= 1) {
        BIP_WARN("%s: entry <= 1, nothing to push\r\n", __func__);
        if (bip->bbmd->push.timer) {
            el_timer_destroy(&el_default_loop, bip->bbmd->push.timer);
            bip->bbmd->push.timer = NULL;
        }
        goto out;
    }

    bip->bbmd->push.idx = 0;
    bip->bbmd->push.each_ms = bip->bbmd->push.interval * 1000 / (bip->bbmd->bdt_size - 1);

    if (bip->bbmd->push.timer == NULL) {
        bip->bbmd->push.timer = el_timer_create(&el_default_loop, 0);
        if (bip->bbmd->push.timer == NULL) {
            BIP_ERROR("%s: create timer failed\r\n", __func__);
            goto out;
        }
        bip->bbmd->push.timer->handler = bdt_push;
        bip->bbmd->push.timer->data = bip;
    } else {
        el_timer_mod(&el_default_loop, bip->bbmd->push.timer, 0);
    }

out:
    RWLOCK_UNLOCK(&(bip->bbmd->bdt_lock));
}

void bdt_push_stop(datalink_bip_t *bip)
{
    if (bip == NULL) {
        BIP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    if (bip->bbmd == NULL) {
        BIP_WARN("%s: bbmd not enable\r\n", __func__);
        return;
    }

    RWLOCK_RDLOCK(&(bip->bbmd->bdt_lock));

    if (bip->bbmd->push.timer) {
        el_timer_destroy(&el_default_loop, bip->bbmd->push.timer);
        bip->bbmd->push.timer = NULL;
    }

    RWLOCK_UNLOCK(&(bip->bbmd->bdt_lock));
}

