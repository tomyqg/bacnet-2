/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * usbmstp.c
 * Original Author:  lincheng, 2015-6-25
 *
 * MSTP
 *
 * History
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <endian.h>

#include "misc/bits.h"
#include "mstp_def.h"
#include "bacnet/network.h"
#include "bacnet/slaveproxy.h"
#include "slaveproxy_def.h"
#include "limits.h"
#include "misc/threadpool.h"

static struct list_head all_mstp_list;

static const uint16_t crc_ccitt_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static inline uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t data)
{
  return (crc >> 8) ^ crc_ccitt_table[(uint8_t)crc ^ data];
}

static uint16_t crc_ccitt(uint16_t crc, uint8_t const *buffer, size_t len)
{
  while (len--)
    crc = crc_ccitt_byte(crc, *buffer++);
  return crc;
}

static const uint32_t crc32k_table[256] = {
0x00000000, 0x9695C4CA, 0xFB4839C9, 0x6DDDFD03, 0x20F3C3CF, 0xB6660705, 0xDBBBFA06, 0x4D2E3ECC,
0x41E7879E, 0xD7724354, 0xBAAFBE57, 0x2C3A7A9D, 0x61144451, 0xF781809B, 0x9A5C7D98, 0x0CC9B952,
0x83CF0F3C, 0x155ACBF6, 0x788736F5, 0xEE12F23F, 0xA33CCCF3, 0x35A90839, 0x5874F53A, 0xCEE131F0,
0xC22888A2, 0x54BD4C68, 0x3960B16B, 0xAFF575A1, 0xE2DB4B6D, 0x744E8FA7, 0x199372A4, 0x8F06B66E,
0xD1FDAE25, 0x47686AEF, 0x2AB597EC, 0xBC205326, 0xF10E6DEA, 0x679BA920, 0x0A465423, 0x9CD390E9,
0x901A29BB, 0x068FED71, 0x6B521072, 0xFDC7D4B8, 0xB0E9EA74, 0x267C2EBE, 0x4BA1D3BD, 0xDD341777,
0x5232A119, 0xC4A765D3, 0xA97A98D0, 0x3FEF5C1A, 0x72C162D6, 0xE454A61C, 0x89895B1F, 0x1F1C9FD5,
0x13D52687, 0x8540E24D, 0xE89D1F4E, 0x7E08DB84, 0x3326E548, 0xA5B32182, 0xC86EDC81, 0x5EFB184B,
0x7598EC17, 0xE30D28DD, 0x8ED0D5DE, 0x18451114, 0x556B2FD8, 0xC3FEEB12, 0xAE231611, 0x38B6D2DB,
0x347F6B89, 0xA2EAAF43, 0xCF375240, 0x59A2968A, 0x148CA846, 0x82196C8C, 0xEFC4918F, 0x79515545,
0xF657E32B, 0x60C227E1, 0x0D1FDAE2, 0x9B8A1E28, 0xD6A420E4, 0x4031E42E, 0x2DEC192D, 0xBB79DDE7,
0xB7B064B5, 0x2125A07F, 0x4CF85D7C, 0xDA6D99B6, 0x9743A77A, 0x01D663B0, 0x6C0B9EB3, 0xFA9E5A79,
0xA4654232, 0x32F086F8, 0x5F2D7BFB, 0xC9B8BF31, 0x849681FD, 0x12034537, 0x7FDEB834, 0xE94B7CFE,
0xE582C5AC, 0x73170166, 0x1ECAFC65, 0x885F38AF, 0xC5710663, 0x53E4C2A9, 0x3E393FAA, 0xA8ACFB60,
0x27AA4D0E, 0xB13F89C4, 0xDCE274C7, 0x4A77B00D, 0x07598EC1, 0x91CC4A0B, 0xFC11B708, 0x6A8473C2,
0x664DCA90, 0xF0D80E5A, 0x9D05F359, 0x0B903793, 0x46BE095F, 0xD02BCD95, 0xBDF63096, 0x2B63F45C,
0xEB31D82E, 0x7DA41CE4, 0x1079E1E7, 0x86EC252D, 0xCBC21BE1, 0x5D57DF2B, 0x308A2228, 0xA61FE6E2,
0xAAD65FB0, 0x3C439B7A, 0x519E6679, 0xC70BA2B3, 0x8A259C7F, 0x1CB058B5, 0x716DA5B6, 0xE7F8617C,
0x68FED712, 0xFE6B13D8, 0x93B6EEDB, 0x05232A11, 0x480D14DD, 0xDE98D017, 0xB3452D14, 0x25D0E9DE,
0x2919508C, 0xBF8C9446, 0xD2516945, 0x44C4AD8F, 0x09EA9343, 0x9F7F5789, 0xF2A2AA8A, 0x64376E40,
0x3ACC760B, 0xAC59B2C1, 0xC1844FC2, 0x57118B08, 0x1A3FB5C4, 0x8CAA710E, 0xE1778C0D, 0x77E248C7,
0x7B2BF195, 0xEDBE355F, 0x8063C85C, 0x16F60C96, 0x5BD8325A, 0xCD4DF690, 0xA0900B93, 0x3605CF59,
0xB9037937, 0x2F96BDFD, 0x424B40FE, 0xD4DE8434, 0x99F0BAF8, 0x0F657E32, 0x62B88331, 0xF42D47FB,
0xF8E4FEA9, 0x6E713A63, 0x03ACC760, 0x953903AA, 0xD8173D66, 0x4E82F9AC, 0x235F04AF, 0xB5CAC065,
0x9EA93439, 0x083CF0F3, 0x65E10DF0, 0xF374C93A, 0xBE5AF7F6, 0x28CF333C, 0x4512CE3F, 0xD3870AF5,
0xDF4EB3A7, 0x49DB776D, 0x24068A6E, 0xB2934EA4, 0xFFBD7068, 0x6928B4A2, 0x04F549A1, 0x92608D6B,
0x1D663B05, 0x8BF3FFCF, 0xE62E02CC, 0x70BBC606, 0x3D95F8CA, 0xAB003C00, 0xC6DDC103, 0x504805C9,
0x5C81BC9B, 0xCA147851, 0xA7C98552, 0x315C4198, 0x7C727F54, 0xEAE7BB9E, 0x873A469D, 0x11AF8257,
0x4F549A1C, 0xD9C15ED6, 0xB41CA3D5, 0x2289671F, 0x6FA759D3, 0xF9329D19, 0x94EF601A, 0x027AA4D0,
0x0EB31D82, 0x9826D948, 0xF5FB244B, 0x636EE081, 0x2E40DE4D, 0xB8D51A87, 0xD508E784, 0x439D234E,
0xCC9B9520, 0x5A0E51EA, 0x37D3ACE9, 0xA1466823, 0xEC6856EF, 0x7AFD9225, 0x17206F26, 0x81B5ABEC,
0x8D7C12BE, 0x1BE9D674, 0x76342B77, 0xE0A1EFBD, 0xAD8FD171, 0x3B1A15BB, 0x56C7E8B8, 0xC0522C72
};

static inline uint32_t crc32k_byte(uint32_t crc, const uint8_t data)
{
    return crc32k_table[(uint8_t)crc ^ data] ^ (crc >> 8);
}

static uint32_t crc32k(uint32_t crc, const uint8_t *buffer, size_t len)
{
    while (len--)
      crc = crc32k_byte(crc, *buffer++);
    return crc;
}

static int cobs_encode(uint8_t *to, const uint8_t *fr, size_t length)
{
	size_t code_index = 0;
	size_t read_index = 0;
	size_t write_index = 1;
	uint8_t code = 1;
	uint8_t data, last_code = 0;

	while (read_index < length) {
		data = fr[read_index++];

		if (data != 0) {
			to[write_index++] = data ^ 0x55;
			code++;
			if (code != 255)
				continue;
		}

		last_code = code;
		to[code_index] = code ^ 0x55;
		code_index = write_index++;
		code = 1;
	}

	if ((last_code == 255) && (code == 1)) {
		write_index--;
	} else {
		to[code_index] = code ^ 0x55;
    }
    
	return write_index;
}

static int frame_encode(uint8_t *to, const uint8_t *fr, size_t length)
{
	size_t cobs_data_len;
	uint32_t crc32k_value, little_endian_value;

	cobs_data_len = cobs_encode(to, fr, length);
	crc32k_value = ~crc32k(0xffffffff, to, cobs_data_len);
	little_endian_value = htole32(crc32k_value);
	
	return cobs_data_len + cobs_encode(to + cobs_data_len, (uint8_t*)&little_endian_value, 4);
}
	
static int cobs_decode(uint8_t *to, const uint8_t *fr, size_t length)
{
	size_t read_index = 0;
	size_t write_index = 0;
	uint8_t code, last_code;

	while (read_index < length) {
		code = fr[read_index] ^ 0x55;
		last_code = code;

		if (read_index + code > length) {
			return -1;
		}
		read_index++;

		while (--code > 0) {
			to[write_index++] = fr[read_index++] ^ 0x55;
		}

		if ((last_code != 255) && (read_index < length)) {
			to[write_index++] = 0;
		}
	}

	return write_index;
}

static inline uint16_t get_packet_usage(uint16_t packet_len)
{
	return (packet_len + OUT_PREFIX_SPACE + 1) & ~1;
}

static inline unsigned get_buffer_left(unsigned size, unsigned head, unsigned tail)
{
	if (head >= tail) {
		if (size - head - 1 >= tail) {
			return size - head - 1;
		} else {
			return tail - 1;
        }
	} else {
		return tail - head - 1;
	}
}

/* return true = send success */
static int __inner_send(usb_mstp_t *mstp, uint8_t *buf, int len)
{
    uint16_t usage_len;
    int rv;

    if (mstp->auto_busy && mstp->auto_mac) {
        return 0;
    }

    if ((uint8_t)(mstp->now_sn - mstp->sent_sn) >= 255) {
        return 0;
    }
    
    usage_len = get_packet_usage(len);
    if (mstp->left_space < usage_len) {
        return 0;
    }
    
    rv = usb_serial_async_write(mstp->serial, buf, len);
    if (rv) {
        MSTP_ERROR("%s: usb serial queue write failed\r\n", __func__);
        return 0;
    }

    mstp->left_space -= usage_len;
    mstp->out_size[mstp->now_sn] = usage_len;
    mstp->now_sn++;
    
    return 1;
}

static void keep_send_packet(usb_mstp_t *mstp)
{
    uint8_t *buf;
    uint16_t len;
    
    if (mstp->test.data != NULL && !mstp->test.sent) {
        if (__inner_send(mstp, mstp->test.data, mstp->test.len)) {
            mstp->test.sent = true;
            mstp->test.sn = mstp->now_sn;
        } else {
            return;
        }
    }

	while (mstp->head != mstp->tail) {
		len = (mstp->out_buf[mstp->tail] << 8) + mstp->out_buf[mstp->tail + 1];
		buf =  mstp->out_buf + mstp->tail + 2;

        if (!__inner_send(mstp, buf, len)) {
            break;
        }
        
        mstp->packet_queued--;
        mstp->tail += len + 2;
        if (mstp->tail == mstp->not_used) {
            mstp->tail = 0;
            mstp->not_used = mstp->base.tx_buf_size;
        }
	}
}

static void reset_packet_mac(usb_mstp_t *mstp)
{
    uint8_t *buf;
    uint16_t len;
    unsigned tail = mstp->tail;

    while (mstp->head != tail) {
        len = (mstp->out_buf[tail] << 8) + mstp->out_buf[tail + 1];
        buf =  mstp->out_buf + tail + 2;
        buf[2] = mstp->base.mac;

        tail += len + 2;
        if (tail == mstp->not_used) {
            tail = 0;
        }
    }
}

static bool read_callback(unsigned long data, unsigned char *buf, unsigned len)
{
	usb_mstp_t *mstp;
    bacnet_addr_t src_addr;
    MSTP_EVENT_TYPE packet_type;
    
    mstp = (usb_mstp_t *)data;
    if (!mstp) {
        MSTP_ERROR("%s: null mstp\r\n", __func__);
        return true;
    }
    
	if (len < 3) {
	    MSTP_ERROR("%s: rx wrong size: %d\n", __func__, len);
		return true;
	}

	packet_type = (MSTP_EVENT_TYPE)buf[0];
	switch(packet_type) {
	case MSTP_EVENT_BACNET:
    case MSTP_EVENT_BACNET_BCST:
	    mstp->base.dl.rx_all++;
	    src_addr.net = 0;
	    src_addr.len = 1;
	    src_addr.adr[0] = buf[1];

	    MSTP_VERBOS("%s: received pdu len(%d) from(%d)\r\n", __func__, len - 2, buf[1]);

        if (len > MSTP_MAX_NE_DATA_LEN + 2) {
            len = cobs_decode(buf + 2, buf + 2, len - 2);
            if (len < 0) {
                MSTP_WARN("%s: cobs decode failed\n", __func__);
                break;
            } else if (len > MSTP_MAX_DATA_LEN) {
                MSTP_WARN("%s: too large packet after cobs decode\n", __func__);
                break;
            } else {
                len = len + 2;
            }
        }

        bacnet_buf_t *b_buf = container_of(buf - BACNET_BUF_HEADROOM, bacnet_buf_t, head[0]);
        b_buf->data = buf + 2;
        b_buf->data_len = len - 2;
        mstp->base.dl.rx_ok++;

        if (packet_type == MSTP_EVENT_BACNET_BCST && mstp->base.proxy->proxy_enable
                && network_receive_mstp_proxy_pdu(mstp->base.dl.port_id, b_buf) < 0) {
            break;
        }

        network_receive_pdu(mstp->base.dl.port_id, b_buf, &src_addr);
        break;

	case MSTP_EVENT_PTY:
        MSTP_ERROR("%s: pty packet from(%d), data length(%d)\n", __func__, buf[1], len - 2);
		break;
    
	case MSTP_EVENT_TEST:
        pthread_mutex_lock(&mstp->mutex);

        if (mstp->test.data != NULL && mstp->test.sent) {
            mstp->test.result = false;
            if (buf[1] != mstp->test.remote) {
                MSTP_ERROR("%s: test remote mac(%d), but reply(%d)\r\n", __func__,
                        mstp->test.remote, buf[1]);
            } else if (mstp->test.len != len + 3) {
                MSTP_ERROR("%s: test remote mac(%d) len(%d), but reply(%d)\r\n", __func__,
                        mstp->test.remote, mstp->test.len - 5, len - 2);
            } else if (memcmp(mstp->test.data + 3, buf + 2, len - 2)) {
                MSTP_ERROR("%s: test remote mac(%d) data len(%d) not match\r\n", __func__,
                        mstp->test.remote, len - 2);
            } else {
                MSTP_VERBOS("%s: test remote mac(%d) data len(%d) success\r\n", __func__,
                        mstp->test.remote, len - 2);
                mstp->test.result = true;
                if (mstp->test.callback) {
                    mstp_test_result_t result = {{mstp->test.remote, true}};
                    if (tp_queue_work(&tp_default_pool,
                            (tp_work_func)mstp->test.callback,
                            mstp->test.context, result._u) < 0) {
                        MSTP_ERROR("%s: queue test response callback fail\r\n", __func__);
                    }
                }
            }
            free(mstp->test.data);
            mstp->test.data = NULL;
            mstp->test.callback = NULL;
            mstp->test.context = NULL;
        } else {
            MSTP_ERROR("%s: no sender test response from remote(%d), len(%d)\r\n", __func__,
                    buf[1], len - 2);
        }

        pthread_mutex_unlock(&mstp->mutex);
		break;

	case MSTP_EVENT_DEBUG:
        if (mstp_dbg_err) {
            printf("%s: debug:", __func__);
            for (int i = 0; i < len - 1; ++i) {
                printf(" %02x", buf[i+1]);
            }
            printf("\n");
        }
        break;

	case MSTP_EVENT_INFO:
	    if (len == 5) {     /* auto config notify */
	        pthread_mutex_lock(&mstp->mutex);

	        uint8_t old_mac = mstp->base.mac;
	        mstp->base.baud = buf[1];
	        mstp->polarity = buf[2];
	        mstp->base.mac = buf[3];
	        mstp->base.max_master = buf[4];
	        mstp->auto_busy = 0;
            MSTP_VERBOS("%s: auto result: baud(%d), polarity(%d), "
                "mac(%d), max_master(%d)\r\n", __func__, mstp->base.baud, mstp->polarity,
                mstp->base.mac, mstp->base.max_master);

            if (old_mac != mstp->base.mac) { // mac changed
                reset_packet_mac(mstp);
            }

            if (mstp->auto_mac) {
                keep_send_packet(mstp);
            }

            pthread_mutex_unlock(&mstp->mutex);
	        break;
	    } else if (len != 14) {
		    MSTP_ERROR("%s: wrong info event length(%d)\n", __func__, len);
			break;
		}

		uint8_t recv_sn = buf[1];
        uint8_t sent_sn = buf[2];
        uint8_t conflictCount = buf[3];
        uint16_t left = buf[4] + (buf[5] << 8);
        uint8_t sendFailCount = buf[6];
        uint8_t noTokenCount = buf[7];
        uint8_t noReplyCount = buf[8];
        uint8_t noPassCount = buf[9];
        uint8_t errCount = buf[10];
        uint8_t dupTokenCount = buf[11];
        uint8_t noTurnCount = buf[12];
        uint8_t paddingCount = buf[13];

        if ((uint8_t)(recv_sn - mstp->recv_sn) > (uint8_t)(mstp->now_sn - mstp->recv_sn)) {
			MSTP_ERROR("%s:: sn out of order: last recv_sn(%d), now(%d), recv_sn(%d)\n", __func__,
			    mstp->recv_sn, mstp->now_sn, recv_sn);
			break;
		}

		pthread_mutex_lock(&mstp->mutex);

		if (mstp->test.data != NULL && mstp->test.sent
		        && (uint8_t)(mstp->test.sn - mstp->sent_sn)
		            <= (uint8_t)(sent_sn - mstp->sent_sn)) {
            if (mstp->test.callback) {
                mstp_test_result_t result = {{mstp->test.remote, false}};
                if (tp_queue_work(&tp_default_pool,
                        (tp_work_func)mstp->test.callback,
                        mstp->test.context, result._u) < 0) {
                    MSTP_ERROR("%s: queue test response callback fail\r\n", __func__);
                }
            }
            free(mstp->test.data);
            mstp->test.data = NULL;
            mstp->test.result = false;
            mstp->test.callback = NULL;
            mstp->test.context = NULL;
		}

		for (uint8_t sn = recv_sn; sn != mstp->now_sn; ++sn) {
			if (left < mstp->out_size[sn]) {
			    MSTP_ERROR("%s:: space overflow on sn(%d), left(%d), packet(%d)\n", __func__,
			        sn, left, mstp->out_size[sn]);
		        pthread_mutex_unlock(&mstp->mutex);
			    goto out;
			} else {
				left -= mstp->out_size[sn];
	        }
		}

		mstp->recv_sn = recv_sn;
        mstp->sent_sn = sent_sn;
        mstp->left_space = left;

		MSTP_VERBOS("%s: now_sn(%d), recv_sn(%d) sent_sn(%d), left space(%d), "
		        "noTokenCount(%d)\n", __func__,
		        mstp->now_sn, recv_sn, sent_sn, left, noTokenCount);

		mstp->conflictCount += conflictCount;
		mstp->sendFailCount += sendFailCount;
		mstp->noTokenCount += noTokenCount;
		mstp->noReplyCount += noReplyCount;
		mstp->noPassCount += noPassCount;
		mstp->errCount += errCount;
		mstp->dupTokenCount += dupTokenCount;
		mstp->noTurnCount += noTurnCount;
		mstp->paddingCount += paddingCount;
        
		keep_send_packet(mstp);
		pthread_mutex_unlock(&mstp->mutex);
		break;

	default:
	    if (mstp_dbg_err) {
	        printf("%s: unknown event type(%d):", __func__, packet_type);
	        for (int i = 0; i < len - 1; ++i) {
	            printf(" %02x", buf[i+1]);
	        }
	        printf("\n");
	    }
		break;
	}

out:
    return true;
}

static int _mstp_send_pdu_(usb_mstp_t *mstp, bacnet_addr_t *dst_mac, uint8_t src_mac,
            bacnet_buf_t *npdu)
{
    uint16_t packet_len;
    uint16_t pdu_len;
    uint8_t dst;

    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0)
            || (npdu->data_len > MSTP_MAX_DATA_LEN)) {
        MSTP_ERROR("%s: invalid npdu\r\n", __func__);
        return -EINVAL;
    }

    pdu_len = npdu->data_len;

    if ((dst_mac == NULL) || (dst_mac->len == 0)) {
        dst = (uint8_t)MSTP_BROADCAST_ADDRESS;
    } else if (dst_mac->len == 1) {
        if (dst_mac->adr[0] == MSTP_BROADCAST_ADDRESS) {
            MSTP_ERROR("%s: unicast to broadcast mac\r\n", __func__);
            return -EINVAL;
        }
        dst = dst_mac->adr[0];
    } else {
        MSTP_ERROR("%s: invalid dst_mac len(%d)\r\n", __func__, dst_mac->len);
        return -EINVAL;
    }

    if (dst == mstp->base.mac) {
        return npdu->data_len;
    }
    
    if (pdu_len <= MSTP_MAX_NE_DATA_LEN) {
        packet_len = pdu_len + 2 + OUT_PACKET_HEADER;
    } else {
        packet_len = pdu_len + (pdu_len + 253)/254 + 5 + OUT_PACKET_HEADER;
    }

    pthread_mutex_lock(&mstp->mutex);

    if (get_buffer_left(mstp->base.tx_buf_size, mstp->head, mstp->tail) < packet_len + 2) {
        pthread_mutex_unlock(&mstp->mutex);
        MSTP_ERROR("%s: write %d bytes overflow\r\n", __func__, pdu_len);
        return -EPERM;
    }

    bool empty = mstp->head == mstp->tail;

    if (mstp->base.tx_buf_size - mstp->head - 1 < packet_len + 2) {
        if (mstp->head == mstp->tail) {
            mstp->tail = 0;
            mstp->not_used = mstp->base.tx_buf_size;
        } else {
            mstp->not_used = mstp->head;
        }
        mstp->head = 0;
    }

    if (pdu_len <= MSTP_MAX_NE_DATA_LEN) {
        uint8_t *buf = npdu->data - OUT_PACKET_HEADER;
        uint16_t crc = ~crc_ccitt(0xffff, npdu->data, pdu_len);
        buf[0] = MSTP_REQ_BACNET;
        buf[1] = dst;
        buf[2] = src_mac;
        buf[packet_len - 2] = crc;
        buf[packet_len - 1] = crc >> 8;
        if (empty && __inner_send(mstp, buf, packet_len)) {
            pthread_mutex_unlock(&mstp->mutex);
            return OK;
        }
        memcpy(mstp->out_buf + mstp->head + 2, buf, packet_len);
    } else {
        mstp->out_buf[mstp->head + 2] = MSTP_REQ_BACNET;
        mstp->out_buf[mstp->head + 3] = dst;
        mstp->out_buf[mstp->head + 4] = src_mac;
        packet_len = frame_encode(mstp->out_buf + mstp->head + 5, npdu->data, pdu_len)
            + OUT_PACKET_HEADER;
        if (empty && __inner_send(mstp, mstp->out_buf + mstp->head + 2, packet_len)) {
            pthread_mutex_unlock(&mstp->mutex);
            return OK;
        }
    }

    mstp->out_buf[mstp->head] = packet_len >> 8;
    mstp->out_buf[mstp->head + 1] = packet_len;
    mstp->head += packet_len + 2;
    mstp->packet_queued++;

    pthread_mutex_unlock(&mstp->mutex);

    return OK;
}

static int mstp_send_pdu(usb_mstp_t *mstp, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu,
            __attribute__ ((unused))bacnet_prio_t prio, bool der)
{
    int rv;

    if (!mstp) {
        MSTP_ERROR("%s: null mstp\r\n", __func__);
        return -EINVAL;
    }

    mstp->base.dl.tx_all++;

    if (mstp->auto_busy && mstp->auto_mac && dst_mac != NULL && dst_mac->len != 0) {
        MSTP_WARN("%s: auto mac config not finished, only broadcast allowed.\r\n", __func__);
        return -EPERM;
    }

    rv = _mstp_send_pdu_(mstp, dst_mac, der ? mstp->base.mac : MSTP_BROADCAST_ADDRESS, npdu);
    if (rv < 0) {
        MSTP_ERROR("%s: send failed\n", __func__);
        return rv;
    }

    if (mstp->base.proxy->proxy_enable && (dst_mac == NULL || dst_mac->len == 0)) {
        network_receive_mstp_proxy_pdu(mstp->base.dl.port_id, npdu);
    }

    mstp->base.dl.tx_ok++;
    
    return OK;
}

/**
 * 假装以src_mac发送给dst_mac
 * @param mstp
 * @param dst_mac
 * @param src_mac, 伪装的地址，不能与自身地址相同
 * @param npdu
 * @param prio
 * @return 成功返回0，失败返回负数
 */
int mstp_fake_pdu(datalink_mstp_t *mstp, bacnet_addr_t *dst_mac, uint8_t src_mac, bacnet_buf_t *npdu,
        __attribute__ ((unused))bacnet_prio_t prio)
{
    int rv;

    if (!mstp) {
        MSTP_ERROR("%s: null mstp\r\n", __func__);
        return -EINVAL;
    }

    mstp->dl.tx_all++;

    if (((usb_mstp_t *)mstp)->auto_busy && ((usb_mstp_t*)mstp)->auto_mac) {
        MSTP_WARN("%s: auto mac config not finished, discard.\r\n", __func__);
        return -EPERM;
    }

    if ((src_mac == mstp->mac) || (src_mac == MSTP_BROADCAST_ADDRESS)) {
        MSTP_ERROR("%s: invalid fake mac(%d), should not be myself or broadcast\r\n", __func__,
            src_mac);
        return -EINVAL;
    }

    rv = _mstp_send_pdu_((usb_mstp_t *)mstp, dst_mac, src_mac, npdu);
    if (rv < 0) {
        MSTP_ERROR("%s: send failed\n", __func__);
        return rv;
    }

    mstp->dl.tx_ok++;

    return OK;
}

static cJSON *mstp_get_mib(datalink_base_t *dl_port)
{
    cJSON *result;
    usb_mstp_t *mstp;

    if (dl_port == NULL) {
        MSTP_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    result = datalink_get_mib(dl_port);

    if (result == NULL) {
        MSTP_ERROR("%s: datalink_get_mib failed\r\n", __func__);
        return NULL;
    }

    mstp = (usb_mstp_t *)dl_port;

    pthread_mutex_lock(&mstp->mutex);

    cJSON_AddNumberToObject(result, "tx_queue",
        mstp->packet_queued + (uint8_t)(mstp->now_sn - mstp->sent_sn));

    if (mstp->auto_baud || mstp->auto_polarity || mstp->auto_mac) {
        cJSON_AddTrueToObject(result, "auto");
        if (mstp->auto_busy) {
            cJSON_AddFalseToObject(result, "auto_complete");
        } else {
            cJSON_AddTrueToObject(result, "auto_complete");
            cJSON_AddNumberToObject(result, "baudrate", mstp->base.baud);
            if (mstp->polarity) {
                cJSON_AddTrueToObject(result, "inv_polarity");
            } else {
                cJSON_AddFalseToObject(result, "inv_polarity");
            }
            cJSON_AddNumberToObject(result, "this_station", mstp->base.mac);
            cJSON_AddNumberToObject(result, "max_master", mstp->base.max_master);
        }
    } else {
        cJSON_AddFalseToObject(result, "auto");
    }

    cJSON_AddNumberToObject(result, "macConflictCount", mstp->conflictCount);
    cJSON_AddNumberToObject(result, "sendConflictCount", mstp->sendFailCount);
    cJSON_AddNumberToObject(result, "noTokenCount", mstp->noTokenCount);
    cJSON_AddNumberToObject(result, "noReplyCount", mstp->noReplyCount);
    cJSON_AddNumberToObject(result, "noPassCount", mstp->noPassCount);
    cJSON_AddNumberToObject(result, "errorCount", mstp->errCount);
    cJSON_AddNumberToObject(result, "dupTokenCount", mstp->dupTokenCount);
    cJSON_AddNumberToObject(result, "noTurnCount", mstp->noTurnCount);
    cJSON_AddNumberToObject(result, "paddingCount", mstp->paddingCount);

    cJSON_AddItemToObject(result, "proxy", slave_proxy_get_mib(mstp->base.proxy));

    pthread_mutex_unlock(&mstp->mutex);
    
    return result;
}

int mstp_init(void)
{
    INIT_LIST_HEAD(&all_mstp_list);

    return OK;
}

int mstp_startup(void)
{
    usb_mstp_t *mstp;
    int rv;

    el_sync(&el_default_loop);

    if (slave_proxy_startup() < 0) {
        MSTP_ERROR("%s: slave proxy startup failed\r\n", __func__);
        goto out;
    }

    list_for_each_entry(mstp, &all_mstp_list, base.mstp_list) {
        mstp->base.dl.tx_all = 0;
        mstp->base.dl.tx_ok = 0;
        mstp->base.dl.rx_all = 0;
        mstp->base.dl.rx_ok = 0;
        
        rv = usb_serial_enable(mstp->serial, true, 0);
        if (rv < 0) {
            MSTP_ERROR("%s: enable device failed\r\n", __func__);
            mstp_stop();
            goto out;
        }

        // send first null packet
        rv = usb_serial_write(mstp->serial, NULL, 0, 2000);
        if (rv < 0) {
            MSTP_ERROR("%s: send first null packet failed\r\n", __func__);
            mstp_stop();
            goto out;
        }

        mstp->head = 0;
        mstp->tail = 0;
        mstp->packet_queued = 0;
        mstp->not_used = mstp->base.tx_buf_size;
        mstp->recv_sn = 0;
        mstp->now_sn = 0;
        mstp->sent_sn = 0;
        mstp->left_space = 0;
        mstp->conflictCount = 0;
        mstp->noReplyCount = 0;
        mstp->noTokenCount = 0;
        mstp->noPassCount = 0;
        mstp->sendFailCount = 0;
        mstp->errCount = 0;
        mstp->dupTokenCount = 0;
        mstp->noTurnCount = 0;
        mstp->paddingCount = 0;

        if (mstp->auto_mac || mstp->auto_baud || mstp->auto_polarity) {
            mstp->auto_busy = 1;
        } else {
            mstp->auto_busy = 0;
        }
        
        for (int i = 0; i < mstp->in_xfr_size; ++i) {
            bacnet_buf_init(&mstp->in_buf[i].buf, MSTP_MAX_DATA_LEN);
            rv = usb_serial_async_read(mstp->serial, mstp->in_buf[i].buf.data,
                IN_MAX_PACKET_SIZE + 1);
            if (rv != 0) {
                MSTP_ERROR("%s: usb serial queue read failed", __func__);
                mstp_stop();
                goto out;
            }
        }
    }

    el_unsync(&el_default_loop);
    MSTP_VERBOS("%s: ok\r\n", __func__);
    mstp_set_dbg_level(0);

    return OK;

out:
    el_unsync(&el_default_loop);
    
    return -EPERM;
}

void mstp_stop(void)
{
    usb_mstp_t *mstp;
    int rv;

    el_sync(&el_default_loop);

    list_for_each_entry(mstp, &all_mstp_list, base.mstp_list) {
        usb_serial_cancel_async(mstp->serial);

        // disable interface
        rv = usb_serial_enable(mstp->serial, false, 0);
        if (rv < 0) {
            MSTP_ERROR("%s: disable interface failed\r\n", __func__);
        }
    }

    slave_proxy_stop();
    
    el_unsync(&el_default_loop);
}

void mstp_clean(void)
{
    usb_mstp_t *each;

    while((each = list_first_entry_or_null(&all_mstp_list, usb_mstp_t, base.mstp_list))) {
        list_del(&each->base.mstp_list);
        free(each->base.proxy);
        free(each->in_buf);
        free(each->out_buf);
        pthread_mutex_destroy(&each->mutex);
        usb_serial_destroy(each->serial);
        free(each);
    }
}

void mstp_exit(void)
{
    mstp_stop();
    mstp_clean();
}

datalink_mstp_t *mstp_port_create(cJSON *cfg, cJSON *res)
{
    usb_mstp_t *mstp;
    cJSON *tmp, *item;
    const char *ifname, *res_type;
    uint8_t enumerated_idx, interface_idx;
    int rv;

    if (cfg == NULL || res == NULL) {
        MSTP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    cfg = cJSON_Duplicate(cfg, true);
    if (cfg == NULL) {
        MSTP_ERROR("%s: cjson duplicate failed\r\n", __func__);
        return NULL;
    }

    mstp = (usb_mstp_t *)malloc(sizeof(usb_mstp_t));
    if (!mstp) {
        MSTP_ERROR("%s: malloc datalink_mstp_t failed\r\n", __func__);
        goto out0;
    }
    memset(mstp, 0, sizeof(usb_mstp_t));

    mstp->base.dl.send_pdu = (int(*)(datalink_base_t *, bacnet_addr_t *, bacnet_buf_t *,
        bacnet_prio_t, bool))mstp_send_pdu;
    mstp->base.dl.get_port_mib = mstp_get_mib;
    mstp->base.dl.max_npdu_len = MSTP_MAX_DATA_LEN;

    rv = pthread_mutex_init(&mstp->mutex, NULL);
    if (rv) {
        MSTP_ERROR("%s: init mstp_mutex failed cause %s\r\n", __func__, strerror(rv));
        goto out1;
    }

    tmp = cJSON_GetObjectItem(cfg, "resource_name");
    if ((!tmp) || (tmp->type != cJSON_String)) {
        MSTP_ERROR("%s: get resource_name item failed\r\n", __func__);
        goto out2;
    }

    res_type = datalink_get_type_by_resource_name(res, tmp->valuestring);
    if (res_type == NULL) {
        MSTP_ERROR("%s: get resource type failed by name: %s\r\n", __func__, tmp->valuestring);
        goto out2;
    }

    if (strcmp(res_type, "USB")) {
        MSTP_ERROR("%s: resource type is not USB: %s\r\n", __func__, res_type);
        goto out2;
    }

    ifname = datalink_get_ifname_by_resource_name(res, tmp->valuestring);
    if (ifname == NULL) {
        MSTP_ERROR("%s: get ifname by resource name:%s failed\r\n", __func__, tmp->valuestring);
        goto out2;
    }
    
    if (sscanf(ifname, "%hhu:%hhu", &enumerated_idx, &interface_idx) != 2) {
        MSTP_ERROR("%s: invalid ifname: %s\r\n", __func__, ifname);
        goto out2;
    }

    cJSON_DeleteItemFromObject(cfg, "resource_name");

    mstp->serial = usb_serial_async_create(enumerated_idx, interface_idx,
            USB_INTERFACE_SUBCLASS_485, USB_INTERFACE_PROTOCOL_MSTP,
            &el_default_loop, read_callback, NULL, NULL, (unsigned long)mstp);
    if (mstp->serial == NULL) {
        MSTP_ERROR("%s: usb serial created failed\r\n", __func__);
        goto out2;
    }

    mstp->base.baud = MSTP_B9600;
    mstp->base.mac = 127;
    mstp->base.max_info_frames = 1;
    mstp->base.max_master = 127;
    mstp->base.reply_timeout = 255;
    mstp->base.usage_timeout = 20;
    mstp->base.tx_buf_size = MIN_BUFFER_SIZE;
    mstp->base.rx_buf_size = MIN_BUFFER_SIZE;
    mstp->reply_fast_timeout = 4;
    mstp->usage_fast_timeout = 1;
    mstp->polarity = 0;
    mstp->auto_baud = 0;
    mstp->auto_polarity = 0;
    mstp->auto_mac = 0;

    tmp = cJSON_GetObjectItem(cfg, "baudrate");
    if (!tmp || (tmp->type != cJSON_Number)) {
        MSTP_ERROR("%s: get baudrate item failed\r\n", __func__);
        goto out3;
    }
    mstp->base.baud = mstp_baudrate2enum(tmp->valueint);
    if (mstp->base.baud == MSTP_B_MAX) {
        MSTP_ERROR("%s: invalid baudrate: %d\r\n", __func__, tmp->valueint);
        goto out3;
    }
    cJSON_DeleteItemFromObject(cfg, "baudrate");

    tmp = cJSON_GetObjectItem(cfg, "max_master");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: max_master item should be number\r\n", __func__);
            goto out3;
        }
        mstp->base.max_master = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "max_master");
    }

    tmp = cJSON_GetObjectItem(cfg, "max_info_frames");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: max_info_frames item should be number\r\n", __func__);
            goto out3;
        }
        if (tmp->valueint <= 0 || tmp->valueint > 255) {
            MSTP_ERROR("%s: max_info_frames item should >0 and <=255\r\n", __func__);
            goto out3;
        }
        
        mstp->base.max_info_frames = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "max_info_frames");
    }
    
    tmp = cJSON_GetObjectItem(cfg, "reply_timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: reply_timeout item should be number\r\n", __func__);
            goto out3;
        }
        mstp->base.reply_timeout = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "reply_timeout");
    }

    tmp = cJSON_GetObjectItem(cfg, "reply_fast_timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: reply_fast_timeout item should be number\r\n", __func__);
            goto out3;
        }
        mstp->reply_fast_timeout = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "reply_fast_timeout");
    }
    
    tmp = cJSON_GetObjectItem(cfg, "usage_timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: usage_timeout item should be number\r\n", __func__);
            goto out3;
        }
        mstp->base.usage_timeout = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "usage_timeout");
    }

    tmp = cJSON_GetObjectItem(cfg, "usage_fast_timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: usage_fast_timeout item should be number\r\n", __func__);
            goto out3;
        }
        mstp->usage_fast_timeout = tmp->valueint;
        cJSON_DeleteItemFromObject(cfg, "usage_fast_timeout");
    }

    tmp = cJSON_GetObjectItem(cfg, "tx_buf_size");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: tx_buf_size item should be number\r\n", __func__);
            goto out3;
        }

        if (tmp->valueint < MIN_BUFFER_SIZE) {
            MSTP_WARN("%s: tx_buf_size too small(%d), use (%d)\r\n", __func__, tmp->valueint,
                MIN_BUFFER_SIZE);
        } else if (tmp->valueint > MAX_BUFFER_SIZE) {
            MSTP_WARN("%s: tx_buf_size too large(%d), use (%d)\r\n", __func__, tmp->valueint,
                MAX_BUFFER_SIZE);
            mstp->base.tx_buf_size = MAX_BUFFER_SIZE;
        } else {
            mstp->base.tx_buf_size = tmp->valueint;
        }
        cJSON_DeleteItemFromObject(cfg, "tx_buf_size");
    }

    tmp = cJSON_GetObjectItem(cfg, "rx_buf_size");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            MSTP_ERROR("%s: rx_buf_size item should be number\r\n", __func__);
            goto out3;
        }

        if (tmp->valueint < MIN_BUFFER_SIZE) {
            MSTP_WARN("%s: rx_buf_size too small(%d), use (%d)\r\n", __func__, tmp->valueint,
                MIN_BUFFER_SIZE);
        } else if (tmp->valueint > MAX_BUFFER_SIZE) {
            MSTP_WARN("%s: rx_buf_size too large(%d), use (%d)\r\n", __func__, tmp->valueint,
                MAX_BUFFER_SIZE);
            mstp->base.rx_buf_size = MAX_BUFFER_SIZE;
        } else {
            mstp->base.rx_buf_size = tmp->valueint;
        }
        cJSON_DeleteItemFromObject(cfg, "rx_buf_size");
    }
    
    tmp = cJSON_GetObjectItem(cfg, "this_station");
    if (!tmp || tmp->type != cJSON_Number) {
        MSTP_ERROR("%s: get this_station item failed\r\n", __func__);
        goto out3;
    }
    mstp->base.mac = tmp->valueint;
    cJSON_DeleteItemFromObject(cfg, "this_station");

    tmp = cJSON_GetObjectItem(cfg, "fast_nodes");
    if (tmp) {
        if (tmp->type != cJSON_Array) {
            MSTP_ERROR("%s: fast_nodes item should be array\r\n", __func__);
            goto out3;
        }

        cJSON_ArrayForEach(item, tmp) {
            if (item->type != cJSON_Number) {
                MSTP_ERROR("%s: item in fast_nodes should be number\r\n", __func__);
                goto out3;
            }
            if ((item->valueint < 0) || (item->valueint >= MSTP_BROADCAST_ADDRESS)) {
                MSTP_ERROR("%s: number in fast_nodes should be 0~254\r\n", __func__);
                goto out3;
            }
            set_bit(mstp->fast_nodes, item->valueint);
        }

        cJSON_DeleteItemFromObject(cfg, "fast_nodes");
    }

    mstp->in_xfr_size = mstp->base.rx_buf_size / 2048;
    mstp->out_buf = malloc(mstp->base.tx_buf_size);
    mstp->in_buf = malloc(bacnet_buf_calsize(MSTP_MAX_DATA_LEN) * mstp->in_xfr_size);
    if ((mstp->in_buf == NULL) || (mstp->out_buf == NULL)) {
        goto out4;
    }
    
    tmp = cJSON_GetObjectItem(cfg, "inv_polarity");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            MSTP_ERROR("%s: invalid inv_polarity\r\n", __func__);
            goto out4;
        }
        if (tmp->type == cJSON_True) {
            mstp->polarity = 1;
        }
        cJSON_DeleteItemFromObject(cfg, "inv_polarity");
    }

    tmp = cJSON_GetObjectItem(cfg, "auto_polarity");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            MSTP_ERROR("%s: invalid auto_polarity\r\n", __func__);
            goto out4;
        }
        if (tmp->type == cJSON_True) {
            mstp->auto_polarity = 1;
        }
        cJSON_DeleteItemFromObject(cfg, "auto_polarity");
    }

    tmp = cJSON_GetObjectItem(cfg, "auto_baudrate");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            MSTP_ERROR("%s: invalid auto_baudrate\r\n", __func__);
            goto out4;
        }
        if (tmp->type == cJSON_True) {
            mstp->auto_baud = 1;
        }
        cJSON_DeleteItemFromObject(cfg, "auto_baudrate");
    }

    tmp = cJSON_GetObjectItem(cfg, "auto_mac");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            MSTP_ERROR("%s: invalid auto_mac\r\n", __func__);
            goto out4;
        }
        if (tmp->type == cJSON_True) {
            mstp->auto_mac = 1;
        }
        cJSON_DeleteItemFromObject(cfg, "auto_mac");
    }

    // set parameter
    mstp_parameter_t para;
    para.baudrate = mstp->base.baud;
    para.mac = mstp->base.mac;
    para.max_info_frames = mstp->base.max_info_frames;
    para.max_master = mstp->base.max_master;
    para.reply_fast_timeout = mstp->reply_fast_timeout;
    para.usage_fast_timeout = mstp->usage_fast_timeout;
    para.reply_timeout = mstp->base.reply_timeout;
    para.usage_timeout = mstp->base.usage_timeout;
    para.polarity = mstp->polarity;
    para.auto_baud = mstp->auto_baud;
    para.auto_polarity = mstp->auto_polarity;
    para.auto_mac = mstp->auto_mac;

    usleep(5000); // wait interface really disabled
    rv = usb_serial_setpara(mstp->serial, BASE_PARAMETER_IDX, (uint8_t*)&para, sizeof(para), 0);
    if (rv < 0) {
        MSTP_ERROR("%s: set parameter failed\r\n", __func__);
        goto out4;
    }

    rv = usb_serial_setpara(mstp->serial, MSTP_PTY_PARAMETER_IDX, mstp->fast_nodes,
        sizeof(mstp->fast_nodes), 0);
    if (rv < 0) {
        MSTP_ERROR("%s: set pty failed\r\n", __func__);
        goto out4;
    }

    // re-get parameter
    rv = usb_serial_getpara(mstp->serial, BASE_PARAMETER_IDX, (uint8_t*)&para, sizeof(para), 0);
    if (rv < 0) {
        MSTP_ERROR("%s: get parameter again from usb failed\r\n", __func__);
        goto out4;
    }
    
    mstp->base.mac = para.mac;
    mstp->base.baud = para.baudrate;
    mstp->base.max_info_frames = para.max_info_frames;
    mstp->base.max_master = para.max_master;
    mstp->reply_fast_timeout = para.reply_fast_timeout;
    mstp->usage_fast_timeout = para.usage_fast_timeout;
    mstp->base.reply_timeout = para.reply_timeout;
    mstp->base.usage_timeout = para.usage_timeout;
    mstp->base.proxy = slave_proxy_port_create(&mstp->base, cfg);
    if (mstp->base.proxy == NULL) {
        MSTP_ERROR("%s: create slave proxy port failed\r\n", __func__);
        goto out4;
    }

    list_add_tail(&(mstp->base.mstp_list), &all_mstp_list);

    cJSON *child = cfg->child;
    while (child) {
        MSTP_WARN("%s: unknown cfg item: %s\r\n", __func__, child->string);
        child = child->next;
    }

    cJSON_Delete(cfg);
    return &mstp->base;

out4:
    if (mstp->in_buf) {
        free(mstp->in_buf);
    }
    if (mstp->out_buf) {
        free(mstp->out_buf);
    }

out3:
    usb_serial_destroy(mstp->serial);

out2:
    pthread_mutex_destroy(&mstp->mutex);

out1:
    free(mstp);

out0:
    cJSON_Delete(cfg);

    return NULL;
}

int mstp_port_delete(datalink_mstp_t *base)
{
    usb_mstp_t *mstp;
    
    if (base == NULL) {
        MSTP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    mstp = (usb_mstp_t *)base;

    list_del(&base->mstp_list);

    slave_proxy_port_delete(base);
    free(mstp->in_buf);
    free(mstp->out_buf);

    usb_serial_destroy(mstp->serial);

    pthread_mutex_destroy(&mstp->mutex);

    free(mstp);

    return OK;
}

datalink_mstp_t *mstp_next_port(datalink_mstp_t *prev)
{
    if (!prev) {
        if (list_empty(&all_mstp_list)) {
            return NULL;
        }
        
        return list_first_entry(&all_mstp_list, datalink_mstp_t, mstp_list);
    }

    if (prev->mstp_list.next != &all_mstp_list) {
        return list_next_entry(prev, mstp_list);
    }

    return NULL;
}

cJSON *mstp_get_status(cJSON *request)
{
    /* TODO */
    return NULL;
}

/* return <0 if no result, 0 = fail, 1 = success */
int mstp_get_test_result(datalink_mstp_t *base, uint8_t *remote)
{
    int rv;
    usb_mstp_t *mstp;

    if (base == NULL) {
        MSTP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    mstp = (usb_mstp_t*)base;

    pthread_mutex_lock(&mstp->mutex);

    if (mstp->test.remote == MSTP_BROADCAST_ADDRESS
            || mstp->test.remote == mstp->base.mac) {
        pthread_mutex_unlock(&mstp->mutex);
        return -EPERM;
    }

    if (mstp->test.data != NULL) {
        pthread_mutex_unlock(&mstp->mutex);
        return -EBUSY;
    }

    rv = mstp->test.result;
    if (remote)
        *remote = mstp->test.remote;

    pthread_mutex_unlock(&mstp->mutex);

    return rv;
}

/* return -1 if fail */
int mstp_test_remote(datalink_mstp_t *base, uint8_t remote, uint8_t *data,
        size_t len, void(*callback)(void*, mstp_test_result_t), void *context)
{
    usb_mstp_t *mstp;

    if (base == NULL || data == NULL) {
        MSTP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (len == 0 || len > MSTP_MAX_NE_DATA_LEN) {
        MSTP_ERROR("%s: too large test data len(%d)\r\n", __func__, len);
        return -EINVAL;
    }

    if (remote == MSTP_BROADCAST_ADDRESS) {
        MSTP_ERROR("%s: can not test broadcast mac\r\n", __func__);
        return -EINVAL;
    }

    mstp = (usb_mstp_t*)base;

    pthread_mutex_lock(&mstp->mutex);

    if (mstp->test.data != NULL || (mstp->auto_busy && mstp->auto_mac)) {
        pthread_mutex_unlock(&mstp->mutex);
        return -EBUSY;
    }

    if (remote == mstp->base.mac) {
        MSTP_ERROR("%s: can not test self\r\n", __func__);
        pthread_mutex_unlock(&mstp->mutex);
        return -EPERM;
    }

    mstp->test.data = (uint8_t*)malloc(len + 5);
    if (mstp->test.data == NULL) {
        MSTP_ERROR("%s: not enough memory\r\n", __func__);
        pthread_mutex_unlock(&mstp->mutex);
        return -ENOMEM;
    }

    memcpy(mstp->test.data + 3, data, len);
    mstp->test.len = len + 5;
    mstp->test.remote = remote;
    mstp->test.callback = callback;
    mstp->test.context = context;

    uint16_t crc = ~crc_ccitt(0xffff, data, len);
    mstp->test.data[0] = MSTP_REQ_TEST;
    mstp->test.data[1] = remote;
    mstp->test.data[2] = mstp->base.mac;
    mstp->test.data[len + 3] = crc;
    mstp->test.data[len + 4] = crc >> 8;

    if (__inner_send(mstp, mstp->test.data, mstp->test.len)) {
        mstp->test.sent = true;
        mstp->test.sn = mstp->now_sn;
    } else {
        mstp->test.sent = false;
    }

    pthread_mutex_unlock(&mstp->mutex);

    return OK;
}

