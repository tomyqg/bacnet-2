/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacapp.c
 * Original Author:  linzhixian, 2015-1-23
 *
 * Bacnet APP Encoding/Decoding
 *
 * History
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"
#include "bacnet/object/device.h"
#include "bacnet/bactext.h"

static int hexstr2byte(char* hexstr, uint8_t *byte, size_t size)
{
    uint32_t value;
    int len = strlen(hexstr);
    if (len % 2 != 0)
        return -EINVAL;

    len >>= 1;
    if (len > size)
        return -EINVAL;

    for (int i=0; i<len; ++i) {
        if (sscanf(hexstr, "%2X", &value) != 1)
            return -EINVAL;

        byte[i] = value;
        hexstr += 2;
    }

    return len;
}

int bacapp_encode_application_data(uint8_t *pdu, BACNET_APPLICATION_DATA_VALUE *value)
{
    int len;

    if ((pdu == NULL) || (value == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    len = 0;

    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        pdu[0] = value->tag;
        len++;
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        len = encode_application_boolean(pdu, value->type.Boolean);
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        len = encode_application_unsigned(pdu, value->type.Unsigned_Int);
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        len = encode_application_signed(pdu, value->type.Signed_Int);
        break;

    case BACNET_APPLICATION_TAG_REAL:
        len = encode_application_real(pdu, value->type.Real);
        break;

    case BACNET_APPLICATION_TAG_DOUBLE:
        len = encode_application_double(pdu, value->type.Double);
        break;

    case BACNET_APPLICATION_TAG_OCTET_STRING:
        len = encode_application_octet_string(pdu, &value->type.Octet_String);
        break;

    case BACNET_APPLICATION_TAG_CHARACTER_STRING:
        len = encode_application_character_string(pdu, &value->type.Character_String);
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        len = encode_application_bitstring(pdu, &value->type.Bit_String);
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        len = encode_application_enumerated(pdu, value->type.Enumerated);
        break;

    case BACNET_APPLICATION_TAG_DATE:
        len = encode_application_date(pdu, &value->type.Date);
        break;

    case BACNET_APPLICATION_TAG_TIME:
        len = encode_application_time(pdu, &value->type.Time);
        break;

    case BACNET_APPLICATION_TAG_OBJECT_ID:
        len = encode_application_object_id(pdu, (int)value->type.Object_Id.type,
            value->type.Object_Id.instance);
        break;

    default:
        APP_ERROR("%s: unknown tag data type(%d)\r\n", __func__, value->tag);
        break;
    }

    return len;
}

int bacapp_encode_context_data(uint8_t *pdu, uint8_t context_tag_number,
        BACNET_APPLICATION_DATA_VALUE *value)
{
    int len;

    if ((pdu == NULL) || (value == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    len = 0;

    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        len = encode_context_null(pdu, context_tag_number);
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        len = encode_context_boolean(pdu, context_tag_number, value->type.Boolean);
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        len = encode_context_unsigned(pdu, context_tag_number, value->type.Unsigned_Int);
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        len = encode_context_signed(pdu, context_tag_number, value->type.Signed_Int);
        break;

    case BACNET_APPLICATION_TAG_REAL:
        len = encode_context_real(pdu, context_tag_number, value->type.Real);
        break;

    case BACNET_APPLICATION_TAG_DOUBLE:
        len = encode_context_double(pdu, context_tag_number, value->type.Double);
        break;

    case BACNET_APPLICATION_TAG_OCTET_STRING:
        len = encode_context_octet_string(pdu, context_tag_number, &value->type.Octet_String);
        break;

    case BACNET_APPLICATION_TAG_CHARACTER_STRING:
        len = encode_context_character_string(pdu, context_tag_number, 
            &value->type.Character_String);
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        len = encode_context_bitstring(pdu, context_tag_number, &value->type.Bit_String);
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        len = encode_context_enumerated(pdu, context_tag_number, value->type.Enumerated);
        break;

    case BACNET_APPLICATION_TAG_DATE:
        len = encode_context_date(pdu, context_tag_number, &value->type.Date);
        break;

    case BACNET_APPLICATION_TAG_TIME:
        len = encode_context_time(pdu, context_tag_number, &value->type.Time);
        break;

    case BACNET_APPLICATION_TAG_OBJECT_ID:
        len = encode_context_object_id(pdu, context_tag_number, (int)value->type.Object_Id.type,
            value->type.Object_Id.instance);
        break;

    default:
        APP_ERROR("%s: unknown tag data type(%d)\r\n", __func__, value->tag);
        break;
    }

    return len;
}

int bacapp_decode_application_data(uint8_t *pdu, uint16_t max_pdu_len, 
        BACNET_APPLICATION_DATA_VALUE *value)
{
    int len;

    if ((pdu == NULL) || (max_pdu_len == 0) || (value == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    value->tag = VALID_APPLICATION_TAG_NUMBER(pdu[0]);
    if (value->tag >= MAX_BACNET_APPLICATION_TAG) {
        APP_ERROR("%s: invalid application tag\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    
    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        len = decode_application_null(pdu);
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        len = decode_application_boolean(pdu, &value->type.Boolean);
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        len = decode_application_unsigned(pdu, &value->type.Unsigned_Int);
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        len = decode_application_signed(pdu, &value->type.Signed_Int);
        break;

    case BACNET_APPLICATION_TAG_REAL:
        len = decode_application_real(pdu, &value->type.Real);
        break;

    case BACNET_APPLICATION_TAG_DOUBLE:
        len = decode_application_double(pdu, &value->type.Double);
        break;

    case BACNET_APPLICATION_TAG_OCTET_STRING:
        len = decode_application_octet_string(pdu, &value->type.Octet_String);
        break;

    case BACNET_APPLICATION_TAG_CHARACTER_STRING:
        len = decode_application_character_string(pdu, &value->type.Character_String);
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        len = decode_application_bitstring(pdu, &value->type.Bit_String);
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        len = decode_application_enumerated(pdu, &value->type.Enumerated);
        break;

    case BACNET_APPLICATION_TAG_DATE:
        len = decode_application_date(pdu, &value->type.Date);
        break;

    case BACNET_APPLICATION_TAG_TIME:
        len = decode_application_time(pdu, &value->type.Time);
        break;

    case BACNET_APPLICATION_TAG_OBJECT_ID:
        len = decode_application_object_id(pdu, &value->type.Object_Id.type,
                &value->type.Object_Id.instance);
        break;

    default:
        APP_ERROR("%s: unknown tag data type(%d)\r\n", __func__, value->tag);
        return -1;
    }

    if (len > max_pdu_len)
        return -1;

    return len;
}

static void fprint_unknown(FILE *stream, uint8_t tag_number, uint8_t *buf, uint32_t size)
{
    fprintf(stream, "%d?", tag_number);
    for (int i = 0; i < size; ++i) {
        fprintf(stream, "%02x", buf[i]);
    }
}

static void fprint_application_value(FILE *stream, BACNET_APPLICATION_DATA_VALUE *value)
{
    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        fprintf(stream, "null");
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        fprintf(stream, "%s", value->type.Boolean ? "true" : "false");
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        fprintf(stream, "u%u", value->type.Unsigned_Int);
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        fprintf(stream, "s%d", value->type.Signed_Int);
        break;

    case BACNET_APPLICATION_TAG_REAL:
        fprintf(stream, "f%f", value->type.Real);
        break;

    case BACNET_APPLICATION_TAG_DOUBLE:
        fprintf(stream, "lf%lf", value->type.Double);
        break;

    case BACNET_APPLICATION_TAG_OCTET_STRING:
        fprintf(stream, "x");
        for (int i=0; i<value->type.Octet_String.length; ++i)
            fprintf(stream, "%02x", value->type.Octet_String.value[i]);
        break;

    case BACNET_APPLICATION_TAG_CHARACTER_STRING:
        if (value->type.Character_String.encoding == CHARACTER_ANSI_X34) {
            fprintf(stream, "c:%.*s", value->type.Character_String.length,
                    value->type.Character_String.value);
        } else {
            fprintf(stream, "c%hux", value->type.Character_String.encoding);
            for (int i=0; i<value->type.Character_String.length; ++i)
                fprintf(stream, "%02x", value->type.Character_String.value[i]);
        }
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        fprintf(stream, "b");
        int bit_number = bitstring_size(&value->type.Bit_String);
        for (int i=0; i<bit_number; ++i)
            fprintf(stream, "%s", bitstring_get_bit(&value->type.Bit_String, i) ? "1" : "0");
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        fprintf(stream, "e%u", value->type.Enumerated);
        break;

    case BACNET_APPLICATION_TAG_DATE:
        fprintf(stream, "d%u.%u.%u.%u ", value->type.Date.year, value->type.Date.month,
                value->type.Date.day, value->type.Date.wday);
        break;

    case BACNET_APPLICATION_TAG_TIME:
        fprintf(stream, "t%u:%u:%u.%u ", value->type.Time.hour, value->type.Time.min,
                value->type.Time.sec, value->type.Time.hundredths);
        break;

    case BACNET_APPLICATION_TAG_OBJECT_ID:
        fprintf(stream, "o%u.%u", value->type.Object_Id.type,
                value->type.Object_Id.instance);
        break;

    default:
        APP_ERROR("%s: unknown tag data type(%d)\r\n", __func__, value->tag);
        break;
    }
}
/*
 * application: null, true, false, u20(unsigne), s-10(signed), f2.5(float),
 * lf3.5(double), x254b5f(octet string),
 * c:iamstring(char string ansi encoding and ascii string),
 * c0:iamstring (char string with ansi encoding and ascii string)
 * cxa2fb92cf(char string with ansi encoding and hex string),
 * c1xa2fb92cf(char string with ansi encoding and hex string),
 * b11011101(bit string), e20(enumerated), d2015.6.23.SUN(date),
 * t15:35:40.12(time), o1.2(object_id)
 * context unknown type: 0?abc9d5(unknown data with 0 context tag)
 * context application value: 0:null(null with 0 context tag),
 *      0:x2534(octet string with 0 context tag)
 * list: a,b,c
 * constructed: 0{ a, b, 1{ c, d }1 }0
 */
void bacapp_fprint_value(FILE *stream, uint8_t *buf, uint32_t size)
{
    BACNET_APPLICATION_DATA_VALUE value;
    uint8_t tag_number;
    bool sep = false;
    int len = 0;

    if ((stream == NULL) || (buf == NULL) || (size == 0)) {
        return;
    }
    
    while(len < size) {
        tag_number = get_tag_number(&buf[len]);
        if (sep && !IS_CLOSING_TAG(buf[len])) {
            fprintf(stream, ", ");
        } else {
            fprintf(stream, " ");
        }
        
        if (IS_OPENING_TAG(buf[len])) {
            sep = false;
        } else {
            sep = true;
        }
        
        if (!IS_CONTEXT_SPECIFIC(buf[len])) {
            int dec_len = bacapp_decode_application_data(&buf[len], size - len, &value);
            if (dec_len < 0) {
                goto decode_err;
            }
            len += dec_len;
            fprint_application_value(stream, &value);
        } else if (IS_OPENING_CLOSING_TAG(buf[len])) {
            len += CONTEXT_TAG_LENGTH(tag_number);
            if (IS_OPENING_TAG(buf[len]))
                fprintf(stream, "%d{", tag_number);
            else
                fprintf(stream, "}%d", tag_number);
        } else {
            BACNET_OCTET_STRING octet_string;
            int dec_len = decode_context_octet_string(&buf[len], tag_number, &octet_string);
            if (dec_len < 0)
                goto decode_err;
            len += dec_len;
            fprint_unknown(stream, tag_number, octet_string.value, octet_string.length);
        }
    }
    return;

decode_err:
    fprintf(stream, "\r\nDecode fail at(%d)\r\n", len);
}

static int snprintf_unknown(char *str, size_t str_len, uint8_t tag_number, uint8_t *buf, uint32_t size)
{
    int tem_len;
    int len;
    int i;
        
    len = snprintf(str, str_len, "%d?", tag_number);
    if ((len < 0) || (len >= str_len)) {
        return -EPERM;
    }

    for (i = 0; i < size; i++) {
        tem_len = snprintf(&str[len], str_len - len, "%02x", buf[i]);
        if ((tem_len < 0) || (tem_len >= str_len - len)) {
            return -EPERM;
        }
        len += tem_len;
    }

    return len;
}

static int snprintf_application_value(char *str, size_t str_len, BACNET_APPLICATION_DATA_VALUE *value)
{
    int tem_len, len;
    int i;

    len = 0;
    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        len = snprintf(str, str_len, "null");
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        len = snprintf(str, str_len, "%s", value->type.Boolean? "true" : "false");
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        len = snprintf(str, str_len, "u%u", value->type.Unsigned_Int);
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        len = snprintf(str, str_len, "s%d", value->type.Signed_Int);
        break;

    case BACNET_APPLICATION_TAG_REAL:
        len = snprintf(str, str_len, "f%f", value->type.Real);
        break;

    case BACNET_APPLICATION_TAG_DOUBLE:
        len = snprintf(str, str_len, "lf%lf", value->type.Double);
        break;

    case BACNET_APPLICATION_TAG_OCTET_STRING:
        len = snprintf(str, str_len, "x");
        if ((len < 0) || (len >= str_len)) {
            return -EPERM;
        }
        
        for (i = 0; i < value->type.Octet_String.length; i++) {
            tem_len = snprintf(&str[len], str_len - len, "%02x", value->type.Octet_String.value[i]);
            if ((tem_len < 0) || (tem_len >= str_len - len)) {
                return -EPERM;
            }
            len += tem_len;
        }
        break;

    case BACNET_APPLICATION_TAG_CHARACTER_STRING:
        if (value->type.Character_String.encoding == CHARACTER_ANSI_X34) {
            len = snprintf(str, str_len, "c:%.*s", value->type.Character_String.length,
                value->type.Character_String.value);
        } else {
            len = snprintf(str, str_len, "c%hux", value->type.Character_String.encoding);
            if ((len < 0) || (len >= str_len)) {
                return -EPERM;
            }
            
            for (i = 0; i < value->type.Character_String.length; i++) {
                tem_len = snprintf(&str[len], str_len - len, "%02x", 
                    value->type.Character_String.value[i]);
                if ((tem_len < 0) || (tem_len >= str_len - len)) {
                    return -EPERM;
                }
                len += tem_len;
            }
        }
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        len = snprintf(str, str_len, "b");
        if ((len < 0) || (len >= str_len)) {
            return -EPERM;
        }
        
        int bit_number = bitstring_size(&value->type.Bit_String);
        for (i = 0; i < bit_number; i++) {
            tem_len = snprintf(&str[len], str_len - len, "%s", 
                bitstring_get_bit(&value->type.Bit_String, i)? "1" : "0");
            if ((tem_len < 0) || (tem_len >= str_len - len)) {
                return -EPERM;
            }
            len += tem_len;
        }
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        len = snprintf(str, str_len, "e%u", value->type.Enumerated);
        break;

    case BACNET_APPLICATION_TAG_DATE:
        len = snprintf(str, str_len, "d%u.%u.%u.%u ", value->type.Date.year, value->type.Date.month,
            value->type.Date.day, value->type.Date.wday);
        break;

    case BACNET_APPLICATION_TAG_TIME:
        len = snprintf(str, str_len, "t%u:%u:%u.%u ", value->type.Time.hour, value->type.Time.min,
            value->type.Time.sec, value->type.Time.hundredths);
        break;

    case BACNET_APPLICATION_TAG_OBJECT_ID:
        len = snprintf(str, str_len, "o%u.%u", value->type.Object_Id.type,
            value->type.Object_Id.instance);
        break;

    default:
        APP_ERROR("%s: unknown tag data type(%d)\r\n", __func__, value->tag);
        return -EPERM;
    }

    if ((len < 0) || (len >= str_len)) {
        return -EPERM;
    }

    return len;
}

bool bacapp_snprint_value(char *str, size_t str_len, uint8_t *buf, uint32_t size)
{
    BACNET_APPLICATION_DATA_VALUE value;
    uint8_t tag_number;
    bool sep;
    int len;
    int tem_len;
    int print_len;

    if ((str == NULL) || (str_len == 0) || (buf == NULL) || (size == 0)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return false;
    }

    sep = false;
    len = 0;
    tem_len = 0;
    print_len = 0;
    while(len < size) {
        if (str_len <= print_len ) {
            return false;
        }
        
        tag_number = get_tag_number(&buf[len]);
        if (sep && !IS_CLOSING_TAG(buf[len])) {
            tem_len = snprintf(&str[print_len], str_len - print_len, ", ");
        } else {
            tem_len = snprintf(&str[print_len], str_len - print_len, " ");
        }

        if ((tem_len < 0) || (tem_len >= str_len - print_len)) {
            return false;
        }
        print_len += tem_len;

        if (IS_OPENING_TAG(buf[len])) {
            sep = false;
        } else {
            sep = true;
        }
        
        if (!IS_CONTEXT_SPECIFIC(buf[len])) {
            tem_len = bacapp_decode_application_data(&buf[len], size - len, &value);
            if (tem_len < 0) {
                return false;
            }
            len += tem_len;
            
            tem_len = snprintf_application_value(&str[print_len], str_len - print_len, &value);
        } else if (IS_OPENING_CLOSING_TAG(buf[len])) {
            len += CONTEXT_TAG_LENGTH(tag_number);
            if (IS_OPENING_TAG(buf[len])) {
                tem_len = snprintf(&str[print_len], str_len - print_len, "%d{", tag_number);
            } else {
                tem_len = snprintf(&str[print_len], str_len - print_len, "}%d", tag_number);
            }
        } else {
            BACNET_OCTET_STRING octet_string;
            tem_len = decode_context_octet_string(&buf[len], tag_number, &octet_string);
            if (tem_len < 0) {
                return false;
            }
            len += tem_len;
            
            tem_len = snprintf_unknown(&str[print_len], str_len - print_len, tag_number,
                octet_string.value, octet_string.length);
        }

        if ((tem_len < 0) || (tem_len >= str_len - print_len)) {
            return false;
        }
        print_len += tem_len;
    }
    
    return true;
}

static char* opening_tag_pat = "^[ \t]*([0-9]+)\\{(.*[^ \t])?[ \t]*$";
static char* closing_tag_pat = "(\\}[0-9]*[ \t]*)+$";
static char* context_unknown_pat = "^[ \t]*([0-9]+)\\?(([0-9a-fA-F][0-9a-fA-F])*)[ \t]*$";
static char* context_appliation_pat = "^[ \t]*([0-9]+):(.*[^ \t])[ \t]*$";

static char* null_pat = "^[ \t]*(null|NULL)[ \t]*$";
static char* boolean_pat = "^[ \t]*((true|TRUE|True)|(false|FALSE|False))[ \t]*$";
static char* object_id_pat = "^[ \t]*[oO]([0-9]+)\\.([0-9]+)[ \t]*$";
static char* unsigned_pat = "^[ \t]*[uU]([0-9]+)[ \t]*$";
static char* signed_pat = "^[ \t]*[sS](-?[0-9]+)[ \t]*$";
static char* real_pat = "^[ \t]*[fF](-?[0-9]+(\\.[0-9]*)?)[ \t]*$";
static char* double_pat = "^[ \t]*(lf|LF)(-?[0-9]+(\\.[0-9]*)?)[ \t]*$";
static char* char_string_pat = "^[ \t]*[cC]([0-9]*)((:(.*[^ \t])?)|(x([0-9a-zA-Z][0-9a-zA-Z])*))[ \t]*$";
static char* octet_string_pat = "^[ \t]*[xX](([0-9a-fA-F][0-9a-fA-F])*)[ \t]*$";
static char* bit_string_pat = "^[ \t]*[bB]([01]*)[ \t]*$";
static char* enumerated_pat = "^[ \t]*[eE]([0-9]+)[ \t]*$";
static char* date_pat = "^[ \t]*[dD]([0-9]{2,4})?\\.([0-9]{1,2})?\\.([0-9]{1,2})?\\.([1-7])?[ \t]*$";
static char* time_pat = "^[ \t]*[tT]([0-9]{1,2})?:([0-9]{1,2})?:([0-9]{1,2})?\\.([0-9]{1,2})?[ \t]*$";

#include "regex.h"

/* used to load the app data struct with the proper data converted from a command line argument */
bool bacapp_parse_application_data(bacnet_buf_t *pdu, char *argv)
{
    regex_t opening_tag_re, closing_tag_re, context_unknown_re,
            context_application_re, null_re, boolean_re,
            object_id_re, unsigned_re, signed_re, real_re,
            double_re, char_string_re, octet_string_re,
            bit_string_re, enumerated_re, date_re, time_re;
    regmatch_t matchptr[10];
    char *save_ptr, *item_ptr;
    bool status;
    uint8_t push_stack[16], pop_stack[16];
    int rv, push_level = 0, pop_level = 0;

    if (regcomp(&opening_tag_re, opening_tag_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile opening tag pattern fail\r\n");
        return false;
    }

    if (regcomp(&closing_tag_re, closing_tag_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile closing tag pattern fail\r\n");
        return false;
    }

    if (regcomp(&context_unknown_re, context_unknown_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile context unknown tag pattern fail\r\n");
        return false;
    }

    if (regcomp(&context_application_re, context_appliation_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile context application tag pattern fail\r\n");
        return false;
    }

    if (regcomp(&null_re, null_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile null pattern fail\r\n");
        return false;
    }

    if (regcomp(&boolean_re, boolean_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile boolean pattern fail\r\n");
        return false;
    }

    if (regcomp(&object_id_re, object_id_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile object_id pattern fail\r\n");
        return false;
    }

    if (regcomp(&unsigned_re, unsigned_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile unsigned pattern fail\r\n");
        return false;
    }

    if (regcomp(&signed_re, signed_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile signed pattern fail\r\n");
        return false;
    }

    if (regcomp(&real_re, real_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile real pattern fail\r\n");
        return false;
    }

    if (regcomp(&double_re, double_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile double pattern fail\r\n");
        return false;
    }

    if (regcomp(&char_string_re, char_string_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile char_string pattern fail\r\n");
        return false;
    }

    if (regcomp(&octet_string_re, octet_string_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile octet_string pattern fail\r\n");
        return false;
    }

    if (regcomp(&bit_string_re, bit_string_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile bit_string pattern fail\r\n");
        return false;
    }

    if (regcomp(&enumerated_re, enumerated_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile enumerated pattern fail\r\n");
        return false;
    }

    if (regcomp(&date_re, date_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile date pattern fail\r\n");
        return false;
    }

    if (regcomp(&time_re, time_pat, REG_EXTENDED)) {
        APP_ERROR("regex compile time pattern fail\r\n");
        return false;
    }

    item_ptr = strtok_r(argv, ",", &save_ptr);
    do {
        unsigned tag_number;
nested:
        status = false;
        rv = regexec(&opening_tag_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            if (sscanf(&item_ptr[matchptr[1].rm_so], "%u", &tag_number) != 1) {
                APP_ERROR("%s: opening tag get tag number failed\r\n", item_ptr);
                break;
            } else if (tag_number > 255) {
                APP_ERROR("%s: opening tag number should be 0~255\r\n", item_ptr);
                break;
            } else if (push_level >= sizeof(push_stack)) {
                APP_ERROR("%s: opening tag too much nested structure\r\n", item_ptr);
                break;
            }
            APP_VERBOS("opening tag(%d)\r\n", tag_number);
            push_stack[push_level++] = tag_number;
            if (matchptr[2].rm_so < 0)
                goto success;
            item_ptr[matchptr[2].rm_eo] = 0;
            item_ptr = &item_ptr[matchptr[2].rm_so];
            goto nested;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec opening tag pattern fail\r\n");
            break;
        }

        rv = regexec(&closing_tag_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            char *tag_part = &item_ptr[matchptr[0].rm_so];
next_closing:
            if (!push_level) {
                APP_ERROR("%s: closing tag overflow\r\n", item_ptr);
                break;
            } else if (tag_part[1] < '0' || tag_part[1] > '9') {
                tag_number = push_stack[push_level - 1];
            } else if (sscanf(tag_part + 1, "%u", &tag_number) != 1) {
                APP_ERROR("%s: closing tag get tag number failed\r\n", tag_part);
                status = false;
                break;
            } else if (tag_number > 255) {
                APP_ERROR("%s: closing tag number should be 0~255\r\n", tag_part);
                break;
            } else if (tag_number != push_stack[push_level - 1]) {
                APP_ERROR("%s: closing tag not match\r\n", tag_part);
                break;
            } else if (pop_level >= sizeof(pop_stack)) {
                APP_ERROR("%s: closing tag pop overflow\r\n", tag_part);
                break;
            } else {
                pop_stack[pop_level++] = tag_number;
            }
            --push_level;
            tag_part = strchr(tag_part + 1, '}');
            if (tag_part)
                goto next_closing;

            bool found = false;
            for (int i=0; i<matchptr[0].rm_so; ++i) {
                if (item_ptr[i] != ' ' && item_ptr[i] != '\t') {
                    item_ptr[matchptr[0].rm_so] = 0;
                    item_ptr += i;
                    found = true;
                    break;
                }
            }
            if (!found)
                goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec closing tag pattern fail\r\n");
            break;
        }

        rv = regexec(&context_unknown_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            if (sscanf(&item_ptr[matchptr[1].rm_so], "%u", &tag_number) != 1) {
                APP_ERROR("%s: unknown get tag number failed\r\n", item_ptr);
                break;
            } else if (tag_number > 255) {
                APP_ERROR("%s: unknown tag number should be 0~255\r\n", item_ptr);
                break;
            }
            APP_VERBOS("context unknown tag: %d, value: %s\r\n", tag_number,
                    &item_ptr[matchptr[2].rm_so]);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec context unknown pattern fail\r\n");
            break;
        }

        int context_specify = false;
        rv = regexec(&context_application_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            if (sscanf(&item_ptr[matchptr[1].rm_so], "%u", &tag_number) != 1) {
                APP_ERROR("%s: application context tag number failed\r\n", item_ptr);
                break;
            } else if (tag_number > 255) {
                APP_ERROR("%s: application context tag number should be 0~255\r\n", item_ptr);
                break;
            }
            APP_VERBOS("context tag(%d): ", tag_number);
            item_ptr = &item_ptr[matchptr[2].rm_so];
            context_specify = true;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec context application pattern fail\r\n");
            break;
        }

        rv = regexec(&null_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            int len;
            if (context_specify)
                len = encode_context_null(pdu->data + pdu->data_len, tag_number);
            else
                len = encode_application_null(pdu->data + pdu->data_len);
            pdu->data_len += len;

            APP_VERBOS("null\r\n");
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec null pattern fail\r\n");
            break;
        }

        rv = regexec(&boolean_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            bool value = matchptr[2].rm_so >= 0;
            int len;
            if (context_specify)
                len = encode_context_boolean(pdu->data + pdu->data_len,
                        tag_number, value);
            else
                len = encode_application_boolean(pdu->data + pdu->data_len, value);
            pdu->data_len += len;

            APP_VERBOS("booean: %s\r\n", value ? "true" : "false");
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec boolean pattern fail\r\n");
            break;
        }

        rv = regexec(&object_id_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            unsigned otype, instance;
            if (sscanf(item_ptr + matchptr[1].rm_so, "%u.%u", &otype, &instance) != 2) {
                APP_ERROR("%s: object_id get type and instance fail\r\n", item_ptr);
                break;
            } else if (otype >= MAX_BACNET_OBJECT_TYPE) {
                APP_ERROR("%s: object_id invalid type\r\n", item_ptr);
                break;
            } else if (instance > BACNET_MAX_INSTANCE) {
                APP_ERROR("%s: object_id invalid instance\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_object_id(pdu->data + pdu->data_len,
                        tag_number, otype, instance);
            else
                len = encode_application_object_id(pdu->data + pdu->data_len,
                        otype, instance);
            pdu->data_len += len;
            APP_VERBOS("object_id type(%u) instance(%u)\r\n", otype, instance);
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec object_id pattern fail\r\n");
            break;
        }

        rv = regexec(&unsigned_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            unsigned unsigned_value;
            if (sscanf(item_ptr + matchptr[1].rm_so, "%u", &unsigned_value) != 1) {
                APP_ERROR("%s: unsigned get value fail\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_unsigned(pdu->data + pdu->data_len,
                        tag_number, unsigned_value);
            else
                len = encode_application_unsigned(pdu->data + pdu->data_len,
                        unsigned_value);
            pdu->data_len += len;
            APP_VERBOS("unsigned(%u)\r\n", unsigned_value);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec unsigned pattern fail\r\n");
            break;
        }

        rv = regexec(&signed_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            int32_t signed_value;
            if (sscanf(item_ptr + matchptr[1].rm_so, "%d", &signed_value) != 1) {
                APP_ERROR("%s: signed get value fail\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_signed(pdu->data + pdu->data_len,
                        tag_number, signed_value);
            else
                len = encode_application_signed(pdu->data + pdu->data_len, signed_value);
            pdu->data_len += len;
            APP_VERBOS("signed(%d)\r\n", signed_value);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec unsigned pattern fail\r\n");
            break;
        }

        rv = regexec(&real_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            float real;
            if (sscanf(item_ptr + matchptr[1].rm_so, "%f", &real) != 1) {
                APP_ERROR("%s: real get value fail\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_real(pdu->data + pdu->data_len, tag_number, real);
            else
                len = encode_application_real(pdu->data + pdu->data_len, real);
            pdu->data_len += len;
            APP_VERBOS("real(%f)\r\n", real);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec real pattern fail\r\n");
            break;
        }

        rv = regexec(&double_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            double double_value;
            if (sscanf(item_ptr + matchptr[2].rm_so, "%lf", &double_value) != 1) {
                APP_ERROR("%s: double get value fail\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_double(pdu->data + pdu->data_len,
                        tag_number, double_value);
            else
                len = encode_application_double(pdu->data + pdu->data_len,
                        double_value);
            pdu->data_len += len;
            APP_VERBOS("double(%lf)\r\n", double_value);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec double pattern fail\r\n");
            break;
        }

        rv = regexec(&char_string_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            unsigned encoding;
            if (matchptr[1].rm_so == matchptr[1].rm_eo) {
                encoding = CHARACTER_ANSI_X34;
            } else if (sscanf(item_ptr + matchptr[1].rm_so, "%u", &encoding) != 1) {
                APP_ERROR("%s: char_strig get encoding fail\r\n", item_ptr);
                break;
            } else if (encoding >= MAX_CHARACTER_STRING_ENCODING) {
                APP_ERROR("%s: char_string unknown encoding(%u)\r\n", item_ptr, encoding);
                break;
            }
            BACNET_CHARACTER_STRING char_string;
            char_string.encoding = encoding;
            if (matchptr[3].rm_so >= 0) {
                char_string.value = item_ptr + matchptr[3].rm_so + 1;
                char_string.length = matchptr[3].rm_eo - matchptr[3].rm_so - 1;
            } else {
                char_string.value = item_ptr + matchptr[5].rm_so + 1;
                char_string.length = matchptr[5].rm_eo - matchptr[5].rm_so - 1;
                char_string.value[char_string.length] = 0;
                char_string.length = hexstr2byte(char_string.value, (uint8_t*)char_string.value,
                        char_string.length >> 1);
            }
            if (pdu->data + pdu->data_len + char_string.length >= pdu->end) {
                APP_ERROR("%s: char_string pdu overflow\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_character_string(pdu->data + pdu->data_len,
                        tag_number, &char_string);
            else
                len = encode_application_character_string(pdu->data + pdu->data_len,
                        &char_string);
            pdu->data_len += len;
            APP_VERBOS("char_string encoding(%u) length(%d)\r\n",
                    encoding, char_string.length);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec char string pattern fail\r\n");
            break;
        }

        rv = regexec(&octet_string_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            uint8_t *value;
            int len;
            value = (uint8_t*)item_ptr + matchptr[1].rm_so;
            len = matchptr[1].rm_eo - matchptr[1].rm_so;
            value[len] = 0;
            len = hexstr2byte((char*)value, value, len>>1);
            if (pdu->data + pdu->data_len + len >= pdu->end) {
                APP_ERROR("%s octet_string pdu overflow\r\n", item_ptr);
                break;
            }
            APP_VERBOS("octet_string length(%d)\r\n", len);
            if (context_specify)
                len = encode_context_raw_octet_string(pdu->data + pdu->data_len,
                        tag_number, value, len);
            else
                len = encode_application_raw_octet_string(pdu->data + pdu->data_len,
                        value, len);
            pdu->data_len += len;
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec octet string pattern fail\r\n");
            break;
        }

        rv = regexec(&bit_string_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            BACNET_BIT_STRING bit_string;
            int bit_size = matchptr[1].rm_eo - matchptr[1].rm_so;
            bit_string.value = (uint8_t*)item_ptr + matchptr[1].rm_so;
            bitstring_resize(&bit_string, bit_size);
            for(int i=0; i<bit_size; ++i) {
                if (bit_string.value[i] == '1')
                    bitstring_set_bit(&bit_string, i, true);
                else
                    bitstring_set_bit(&bit_string, i, false);
            }
            if (pdu->data + pdu->data_len + bit_string.byte_len >= pdu->end) {
                APP_ERROR("%s bit_string pdu overflow\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_bitstring(pdu->data + pdu->data_len, tag_number,
                        &bit_string);
            else
                len = encode_application_bitstring(pdu->data + pdu->data_len, &bit_string);
            pdu->data_len += len;
            APP_VERBOS("bit_string length(%d)\r\n", bit_size);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec bit string pattern fail\r\n");
            break;
        }

        rv = regexec(&enumerated_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            unsigned unsigned_value;
            if (sscanf(item_ptr + matchptr[1].rm_so, "%u", &unsigned_value) != 1) {
                APP_ERROR("%s: enumerated get value fail\r\n", item_ptr);
                break;
            }
            int len;
            if (context_specify)
                len = encode_context_enumerated(pdu->data + pdu->data_len,
                        tag_number, unsigned_value);
            else
                len = encode_application_enumerated(pdu->data + pdu->data_len,
                        unsigned_value);
            pdu->data_len += len;
            APP_VERBOS("enumerated(%u)\r\n", unsigned_value);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec enumerated pattern fail\r\n");
            break;
        }

        rv = regexec(&date_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            BACNET_DATE date;
            unsigned year, month, day, week;
            if (matchptr[1].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[1].rm_so, "%u", &year) != 1) {
                    APP_ERROR("%s: date get year fail\r\n", item_ptr);
                    break;
                } else if (year < 1900 || year > 1900 + 254) {
                    APP_ERROR("%s: date invalid year\r\n", item_ptr);
                    break;
                }
                year -= 1900;
            } else
                year = 0x0ff;

            if (matchptr[2].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[2].rm_so, "%u", &month) != 1) {
                    APP_ERROR("%s: date get month fail\r\n", item_ptr);
                    break;
                } else if (month < 1 || month > 12) {
                    APP_ERROR("%s: date invalid month\r\n", item_ptr);
                    break;
                }
            } else
                month = 0x0ff;

            if (matchptr[3].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[3].rm_so, "%u", &day) != 1) {
                    APP_ERROR("%s: date get day fail\r\n", item_ptr);
                    break;
                } else if (day < 1 || day > 31) {
                    APP_ERROR("%s: date invalid day\r\n", item_ptr);
                    break;
                }
            } else
                day = 0x0ff;

            if (matchptr[4].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[4].rm_so, "%u", &week) != 1) {
                    APP_ERROR("%s: date get weekday fail\r\n", item_ptr);
                    break;
                } else if (week < 1 || week > 7) {
                    APP_ERROR("%s: date invalid weekday\r\n", item_ptr);
                    break;
                }
            } else
                week = 0x0ff;
            date.year = year; date.month = month;
            date.day = day; date.wday = week;
            int len;
            if (context_specify)
                len = encode_context_date(pdu->data + pdu->data_len, tag_number, &date);
            else
                len = encode_application_date(pdu->data + pdu->data_len, &date);
            pdu->data_len += len;
            APP_VERBOS("date %u.%u.%u.%u\r\n", year, month, day, week);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec date pattern fail\r\n");
            break;
        }

        rv = regexec(&time_re, item_ptr, sizeof(matchptr)/sizeof(matchptr[0]), matchptr, 0);
        if (rv == 0) {
            BACNET_TIME time;
            unsigned hour, minute, second, hundredth;
            if (matchptr[1].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[1].rm_so, "%u", &hour) != 1) {
                    APP_ERROR("%s: time get hour fail\r\n", item_ptr);
                    break;
                } else if (hour > 23) {
                    APP_ERROR("%s: time invalid hour\r\n", item_ptr);
                    break;
                }
            } else
                hour = 0x0ff;

            if (matchptr[2].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[2].rm_so, "%u", &minute) != 1) {
                    APP_ERROR("%s: time get minute fail\r\n", item_ptr);
                    break;
                } else if (minute > 59) {
                    APP_ERROR("%s: time invalid minute\r\n", item_ptr);
                    break;
                }
            } else
                minute = 0x0ff;

            if (matchptr[3].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[3].rm_so, "%u", &second) != 1) {
                    APP_ERROR("%s: time get second fail\r\n", item_ptr);
                    break;
                } else if (second > 59) {
                    APP_ERROR("%s: time invalid second\r\n", item_ptr);
                    break;
                }
            } else
                second = 0x0ff;

            if (matchptr[4].rm_so >= 0) {
                if (sscanf(item_ptr + matchptr[4].rm_so, "%u", &hundredth) != 1) {
                    APP_ERROR("%s: time get hundredth second fail\r\n", item_ptr);
                    break;
                } else if (hundredth > 99) {
                    APP_ERROR("%s: time invalid hundredth second\r\n", item_ptr);
                    break;
                }
            } else
                hundredth = 0x0ff;

            time.hour = hour; time.min = minute;
            time.sec = second; time.hundredths = hundredth;
            int len;
            if (context_specify)
                len = encode_context_time(pdu->data + pdu->data_len, tag_number, &time);
            else
                len = encode_application_time(pdu->data + pdu->data_len, &time);
            pdu->data_len += len;
            APP_VERBOS("time %02u:%02u:%02u.%02u\r\n", hour, minute, second, hundredth);
            goto success;
        } else if (rv != REG_NOMATCH) {
            APP_ERROR("regex exec time pattern fail\r\n");
            break;
        }

        APP_ERROR("%s: parse failed at\r\n", item_ptr);
        status = false;
        break;
success:
        if (pdu->data + pdu->data_len > pdu->end) {
            APP_ERROR("pdu overflow\r\n");
            break;
        }
        if (pop_level) {
            for (int i=0; i<pop_level; ++i) {
                APP_VERBOS("closing tag(%d)\r\n", pop_stack[i]);
                int len = encode_closing_tag(pdu->data + pdu->data_len, pop_stack[i]);
                pdu->data_len += len;
            }
            if (pdu->data + pdu->data_len > pdu->end) {
                APP_ERROR("pdu overflow\r\n");
                break;
            }
            pop_level = 0;
        }
        status = true;
    }while((item_ptr = strtok_r(NULL, ",", &save_ptr)));

    regfree(&opening_tag_re);
    regfree(&closing_tag_re);
    regfree(&context_unknown_re);
    regfree(&context_application_re);
    regfree(&null_re);
    regfree(&boolean_re);
    regfree(&object_id_re);
    regfree(&unsigned_re);
    regfree(&signed_re);
    regfree(&real_re);
    regfree(&double_re);
    regfree(&char_string_re);
    regfree(&octet_string_re);
    regfree(&bit_string_re);
    regfree(&enumerated_re);
    regfree(&date_re);
    regfree(&time_re);

    return status;
}

