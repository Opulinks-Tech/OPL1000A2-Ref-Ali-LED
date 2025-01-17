/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <string.h>
#include <breeze_hal_os.h>
#include "extcmd.h"
#include "auth.h"
#include "common.h"
#include "core.h"
#include "utils.h"
#include "..\src\breeze\include\sha256.h"
#include "breeze_export.h"
//#include "chip_code.h"
#include "bzopt.h"
#include <breeze_hal_sec.h>
#include "blewifi_server_app.h"
#include "blewifi_ctrl.h"
#include "passwd.h"
#include "infra_config.h"
#include "infra_compat.h"
#include "blewifi_configuration.h"

int g_nRegion_Id = 0; //After binding, need to save again.

#define RANDOM_LEN 16
#define SHA256_DATA_LEN 32

#define SSID_READY 0x01
#define PASSWORD_READY 0x02
#define BSSID_READY 0x04
#define APPTOKEN_READY 0x08
#define REGION_ID_READY                         0x10
#define REGION_MQTT_URL_READY                   0x20

#define BASIC_READY (SSID_READY | PASSWORD_READY)
#define ALL_READY (SSID_READY | PASSWORD_READY | BSSID_READY | APPTOKEN_READY)

#define UTF8_MAX_SSID     32
#define UTF8_MAX_PASSWORD 64

#undef printf
#define printf(...)
enum {
    BLE_AWSS_CTYPE_SSID     = 0x01,
    BLE_AWSS_CTYPE_PASSWORD = 0x02,
    BLE_AWSS_CTYPE_BSSID    = 0x03,
    BLE_AWSS_CTYPE_APPTOKEN = 0x04,
    BLE_AWSS_CTYPE_REGION_ID        = 0x05,
    BLE_AWSS_CTYPE_REGION_MQTTURL   = 0x06,
    BLE_AWSS_CTYPE_3B_RANDOM        = 0x07,
    BLE_AWSS_CTYPE_TOKEN_TYPE       = 0x08,
    BLE_AWSS_CTYPE_PROTOCOL_VER     = 0xF1,
};

SHM_DATA breeze_apinfo_t comboinfo;
SHM_DATA extcmd_t g_extcmd;
#ifdef ALI_APP_TOKEN_BACKUP
extern uint8_t g_u8aAppToken[RANDOM_MAX_LEN];
#endif

enum {
    BZ_AUTH_INFO_AUTH_CODE          = 0x01,
    BZ_AUTH_INFO_AUTH_SECRET        = 0x02,
    BZ_AUTH_INFO_PROTOCOL_VER       = 0xF1,
};

enum {
    BZ_AUTH_SIGN_ACCESS_KEY         = 0x01,
    BZ_AUTH_SIGN_RANDOM             = 0x02,
    BZ_AUTH_SIGN_AUTH_SIGN          = 0x03,
    BZ_AUTH_SIGN_PROTOCOL_VER       = 0xF1,
};

typedef ret_code_t (*ext_tlv_handler_t)(uint8_t *p_buff, uint8_t *p_blen,
                                        const uint8_t *p_data, uint8_t dlen);
typedef struct {
    uint8_t tlv_type;
    ext_tlv_handler_t handler;
} ext_tlv_type_handler_t;

extern core_t g_core;
#if BZ_ENABLE_AUTH
extern auth_t g_auth;
#endif

extern ret_code_t auth_get_device_secret(uint8_t *p_secret, int *p_length);

extcmd_t g_extcmd;
breeze_apinfo_t comboinfo;
static SHM_DATA uint8_t g_auth_kv_val[16 + 32 + 2] = {0};
static int g_auth_kv_val_len = 0;
static uint8_t g_auth_need_set = 0;
//const static char m_sdk_version[] = ":" BZ_VERSION;

static ret_code_t ext_cmd01_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd02_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd03_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd04_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd05_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);

#if BZ_ENABLE_COMBO_NET
static ret_code_t ext_cmd06_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
#endif

#ifdef CONFIG_AIS_SECURE_ADV
static ret_code_t ext_cmd07_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
#endif

#ifdef CONFIG_SEC_PER_PK_TO_DN
static ret_code_t ext_cmd09_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd0A_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
#endif

static ret_code_t ext_cmd0B_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);
static ret_code_t ext_cmd0C_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen);

static SHM_DATA const ext_tlv_type_handler_t
  m_tlv_type_handler_table[] = /**< TLV type handler table. */
  { { 0x01, ext_cmd01_rsp}, 
    { 0x02, ext_cmd02_rsp},
    { 0x03, ext_cmd03_rsp}, 
    { 0x04, ext_cmd04_rsp},
    { 0x05, ext_cmd05_rsp},
#if BZ_ENABLE_COMBO_NET
    { 0x06, ext_cmd06_rsp},
#endif
#ifdef CONFIG_AIS_SECURE_ADV
    { 0x07, ext_cmd07_rsp},
#endif
#ifdef CONFIG_SEC_PER_PK_TO_DN
    { 0x09, ext_cmd09_rsp},
    { 0x0A, ext_cmd0A_rsp},
#endif
    { 0x0B, ext_cmd0B_rsp},
    { 0x0C, ext_cmd0C_rsp},
  };

static SHM_DATA ret_code_t ext_cmd01_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_ENOMEM;
    uint8_t len;

    if (dlen > 0) {
        err_code = BZ_EDATASIZE;
    } else if ((len = g_extcmd.tlv_01_rsp_len) <= *p_blen) {
        memcpy(p_buff, g_extcmd.tlv_01_rsp, len);
        *p_blen = len;
        err_code = BZ_SUCCESS;
    }

    return err_code;
}

static SHM_DATA ret_code_t ext_cmd02_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_ENOMEM;
    uint8_t len;

    if (dlen > 0) {
        err_code = BZ_EDATASIZE;
    } else if (g_extcmd.product_key_len == 0) {
        err_code = BZ_ENOTSUPPORTED;
    } else if ((len = g_extcmd.product_key_len) <= *p_blen) {
        memcpy(p_buff, g_extcmd.p_product_key, len);
        *p_blen = len;

        err_code = BZ_SUCCESS;
    }

    return err_code;
}


static SHM_DATA ret_code_t ext_cmd03_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_ENOMEM;
    uint8_t len;

    if (dlen > 0) {
        err_code = BZ_EDATASIZE;
    } else if (g_extcmd.device_name_len == 0) {
        err_code = BZ_ENOTSUPPORTED;
    } else if ((len = g_extcmd.device_name_len) <= *p_blen) {
        memcpy(p_buff, g_extcmd.p_device_name, len);
        *p_blen = len;

        err_code = BZ_SUCCESS;
    }

    return err_code;
}

static SHM_DATA ret_code_t ext_cmd04_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_ENOMEM;

    if (dlen > 0) {
        err_code = BZ_EDATASIZE;
    } else if (g_extcmd.device_secret_len == 0) {
        err_code = BZ_ENOTSUPPORTED;
    } else if (BZ_DEV_RANDOM_LEN <= *p_blen) {
        get_random_ali(p_buff, BZ_DEV_RANDOM_LEN);
        *p_blen = BZ_DEV_RANDOM_LEN;
        err_code = BZ_SUCCESS;
    }
    return err_code;
}

static SHM_DATA void network_signature_calculate(uint8_t *p_buff)
{
    uint8_t str_id[8];//, n;
//    uint8_t random_str[32];
    SHA256_CTX context;
    uint8_t cli_id[4];

    SET_U32_BE(cli_id, g_extcmd.model_id);
    hex2string(cli_id, sizeof(cli_id), str_id);
    sha256_init(&context);

    sha256_update(&context, BZ_CLIENTID_STR, strlen(BZ_CLIENTID_STR));
    sha256_update(&context, str_id, sizeof(str_id));
    BREEZE_VERBOSE("%.*s", sizeof(str_id), str_id);

    sha256_update(&context, BZ_DEVICE_NAME_STR, strlen(BZ_DEVICE_NAME_STR)); /* "deviceName" */
    sha256_update(&context, g_extcmd.p_device_name, g_extcmd.device_name_len);
    BREEZE_VERBOSE("%.*s", g_extcmd.device_name_len, g_extcmd.p_device_name);

#if BZ_ENABLE_AUTH
    if(g_auth.dyn_update_device_secret == true || g_auth.device_secret_len == BZ_DEV_DEVICE_SECRET_LEN){
        sha256_update(&context, BZ_DEVICE_SECRET_STR, strlen(BZ_DEVICE_SECRET_STR)); /* "deviceSecret" */
        sha256_update(&context, g_auth.device_secret, g_auth.device_secret_len);
        BREEZE_VERBOSE("%.*s", g_auth.device_secret_len, g_auth.device_secret);
    } else{
        sha256_update(&context, BZ_PRODUCT_SECRET_STR, strlen(BZ_PRODUCT_SECRET_STR)); /* "productSecret" */
        sha256_update(&context, g_core.product_secret, g_core.product_secret_len);
        BREEZE_VERBOSE("%.*s", g_core.product_secret_len, g_core.product_secret);
    }
#else
    sha256_update(&context, BZ_DEVICE_SECRET_STR, strlen(BZ_DEVICE_SECRET_STR)); /* "deviceSecret" */
    sha256_update(&context, g_extcmd.p_device_secret, g_extcmd.device_secret_len);
#endif

    sha256_update(&context, BZ_PRODUCT_KEY_STR, strlen(BZ_PRODUCT_KEY_STR)); /* "productKey" */
    sha256_update(&context, g_extcmd.p_product_key, g_extcmd.product_key_len);
    BREEZE_VERBOSE("%.*s", g_extcmd.product_key_len, g_extcmd.p_product_key);

    sha256_final(&context, p_buff);
}

static SHM_DATA ret_code_t ext_cmd05_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_ENOMEM;

    if (dlen > 0) {
        err_code = BZ_EDATASIZE;
    } else if (*p_blen >= SHA256_BLOCK_SIZE) {
        network_signature_calculate(p_buff);
        *p_blen = SHA256_BLOCK_SIZE;
        err_code = BZ_SUCCESS;
    }

    return err_code;
}

#if BZ_ENABLE_COMBO_NET
static ret_code_t ext_cmd06_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    uint8_t idx = 0, tlvtype, tlvlen;
    static uint8_t ready_flag = 0;
    uint8_t rsp[] = { 0x01, 0x01, 0x01, 0x03, 0x01, 0x00 };

    if (dlen < 2) {
        err_code = BZ_EDATASIZE;
        goto end;
    }

    if (*p_blen < sizeof(rsp)) {
        err_code = BZ_ENOMEM;
        goto end;
    }

    memset(&comboinfo, 0, sizeof(comboinfo));
    comboinfo.region_type = 0xFF;
    while (idx < dlen) {
        tlvtype = p_data[idx++];
        tlvlen  = p_data[idx++];

        switch (tlvtype) {
            case BLE_AWSS_CTYPE_SSID: /* utf8 */
                if (tlvlen < 1 || tlvlen > UTF8_MAX_SSID) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                utf8_to_pw((uint8_t*)&p_data[idx], tlvlen, comboinfo.ssid);
                ready_flag |= SSID_READY;
                break;
            case BLE_AWSS_CTYPE_PASSWORD: /* utf8 */
                if (tlvlen > UTF8_MAX_PASSWORD) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                utf8_to_pw((uint8_t*)&p_data[idx], tlvlen, comboinfo.pw);
                ready_flag |= PASSWORD_READY;
                break;
            case BLE_AWSS_CTYPE_BSSID: /* 6-byte hex */
                if (tlvlen != 6) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(comboinfo.bssid, &p_data[idx], tlvlen);
                ready_flag |= BSSID_READY;
                break;
            case BLE_AWSS_CTYPE_APPTOKEN: /*16 bytes hex */
                if ((tlvlen > MAX_TOKEN_PARAM_LEN)||
                    (!ready_flag & BSSID_READY)) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                comboinfo.apptoken_len = tlvlen;
                memcpy(comboinfo.apptoken, &p_data[idx], tlvlen);

                #ifdef ALI_APP_TOKEN_BACKUP
                if(tlvlen <= RANDOM_MAX_LEN)
                {
                    memcpy(g_u8aAppToken, &p_data[idx], tlvlen);
                }
                #endif

                ready_flag |= APPTOKEN_READY;
                break;
            case BLE_AWSS_CTYPE_REGION_ID:
                if (tlvlen != 1) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(&(comboinfo.region_id), &p_data[idx], tlvlen);
                comboinfo.region_type = 0;
                break;
            case BLE_AWSS_CTYPE_REGION_MQTTURL:
                if (tlvlen >= 128) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(comboinfo.region_mqtturl, &p_data[idx], tlvlen);
                comboinfo.region_type = 1;
                break;
            case BLE_AWSS_CTYPE_3B_RANDOM:
                if (tlvlen != 3) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(&(comboinfo.rand), &p_data[idx], tlvlen);
                break;
            case BLE_AWSS_CTYPE_TOKEN_TYPE:
                if (tlvlen != 1) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(&(comboinfo.token_type), &p_data[idx], tlvlen);
                break;
            case BLE_AWSS_CTYPE_PROTOCOL_VER:
                if (tlvlen != 1) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                memcpy(&(comboinfo.protocol_ver), &p_data[idx], tlvlen);
                break;
            default:
                BREEZE_DEBUG("unknown apinfo type");
                break;
        }

        idx += tlvlen;
    }

end:
    if (err_code != BZ_SUCCESS) {
        rsp[2] = 2; /* set failure code */
    }
    rsp[5] = comboinfo.token_type;

    /* rsp */
    memcpy(p_buff, rsp, sizeof(rsp));
    *p_blen = sizeof(rsp);

    if (ready_flag & BASIC_READY) {
        core_event_notify(BZ_EVENT_APINFO, (uint8_t*)&comboinfo, sizeof(comboinfo));
			uint8_t *data;
			int data_len;
			uint8_t *pos;
			int malloc_size = (sizeof(breeze_apinfo_t)+3);

			pos = data = malloc(malloc_size);
			if (data == NULL) {
				printf("malloc error\r\n");
				return BZ_ENOMEM;
			}

			uint8_t len = strlen(comboinfo.ssid); //ap_list[i].ssid_length;
			data_len = (pos - data);

			*pos++ = len;
			memcpy(pos, comboinfo.ssid, len);
			pos += len;
			*pos++ = 6;
			memcpy(pos, comboinfo.bssid,6);
			pos += 6;
            
			*pos++ = strlen(comboinfo.pw);
			memcpy(pos, comboinfo.pw, strlen(comboinfo.pw));
			pos += strlen(comboinfo.pw);
            
            *pos++ = comboinfo.apptoken_len;
            memcpy(pos, (const char*)(comboinfo.apptoken), comboinfo.apptoken_len);
            pos += comboinfo.apptoken_len;
			data_len = (pos - data);
            
           
#ifdef ALI_OPL_DBG
            printf("Get apptoken data(%d):", comboinfo.apptoken_len);
            for(int i=0;i<comboinfo.apptoken_len;i++){						
                printf("0x%02x ", comboinfo.apptoken[i]);				
            }
            printf("\r\n");
#endif            
            
#ifdef ALI_BLE_WIFI_PROVISION
			BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_ALI_WIFI_CONNECT, data, data_len);
//            awss_set_token(comboinfo.apptoken);
            awss_set_token(comboinfo.apptoken, (bind_token_type_t)comboinfo.token_type);
            
            // get region information
            if (comboinfo.region_type == REGION_TYPE_ID) {
                printf("[%s, %d]comboinfo.region_id=%d\n", __FUNCTION__, __LINE__, comboinfo.region_id);
                g_nRegion_Id = comboinfo.region_id;
                iotx_guider_set_dynamic_region(comboinfo.region_id);

                extern volatile uint8_t g_u8IotUnbind;

                g_u8IotUnbind = 1;
                BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_LINK_CONN, false);
                BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_UNBIND, true);
                BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_NETWORKING_START, NULL, 0);
            } else if (comboinfo.region_type == REGION_TYPE_MQTTURL) {
//                printf("[%s, %d]comboinfo.region_mqtturl=%s\n", __FUNCTION__, __LINE__, comboinfo.region_mqtturl);
                iotx_guider_set_dynamic_mqtt_url(comboinfo.region_mqtturl);
            } else {
//                printf("REGION TYPE not supported\n\n");
                iotx_guider_set_dynamic_region(IOTX_CLOUD_REGION_INVALID);
            }
            
            
#endif
			free(data);
    }

    return err_code;
}
#endif

#ifdef CONFIG_AIS_SECURE_ADV
void set_adv_sequence(uint32_t seq);
static ret_code_t ext_cmd07_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    uint8_t ret_code = 1, i = 0;
    uint32_t seq = 0;

    if (dlen != sizeof(seq)) {
        BREEZE_ERR("Error: invalid sequence data size");
        err_code = BZ_EDATASIZE;
        goto end;
    }

    if (*p_blen < 1) {
        BREEZE_ERR("Error: invalid sequence rsp buffer size");
        err_code = BZ_ENOMEM;
        goto end;
    }

    BREEZE_DEBUG("The sequence bytes from cloud: %02x %02x %02x %02x", p_data[0],
           p_data[1], p_data[2], p_data[3]);

    while (i < sizeof(seq)) {
        seq |= p_data[i] << (i << 3);
        i++;
    }

    BREEZE_DEBUG("The sequence hex to be saved is %08x", seq);
    set_adv_sequence(seq);

end:
    if (err_code != BZ_SUCCESS) {
        ret_code = 0; /* set failure code */
    }

    /* rsp */
    p_buff[0] = ret_code;
    *p_blen   = 1;

    return err_code;
}
#endif

#ifdef CONFIG_SEC_PER_PK_TO_DN
bool ext_get_device_secret()
{
    char tmp_secret[BZ_DEV_DEVICE_SECRET_LEN];
    int len = BZ_DEV_DEVICE_SECRET_LEN;
    if(auth_get_device_secret((uint8_t*)tmp_secret, &len) == 0){
        if(g_auth.device_secret_len == 0){
            g_auth.device_secret_len = len;
            memcpy(g_auth.device_secret, tmp_secret, len);
        } else if(len == BZ_DEV_DEVICE_SECRET_LEN && memcmp(tmp_secret, g_auth.device_secret, len) != 0){
            memcpy(g_auth.device_secret, tmp_secret, len);
            g_auth.dyn_update_device_secret = true;
        }
        return true;
    } else{
        return false;
    }
}

bool ext_set_device_secret(const uint8_t* p_ds, uint8_t ds_len)
{
    return (os_kv_set(BZ_DEVICE_SECRET_STR, p_ds, ds_len, 1) == 0)? true: false;
}

bool ext_del_device_secret(void)
{
    return (os_kv_del(BZ_DEVICE_SECRET_STR) == 0)? true: false;
}

static ret_code_t ext_cmd09_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    if (dlen > 0 ) {
        BREEZE_ERR("Err:invalid product security update param");
        err_code = BZ_EDATASIZE;
        goto end;
    }

    if (*p_blen < 1) {
        BREEZE_ERR("Err:invalid product security update rsp buffer size");
        err_code = BZ_ENOMEM;
        goto end;
    }

    if(ext_get_device_secret() == false){
        p_buff[0] = 0x01;
    } else{
        p_buff[0] = 0x00;
    }
    memcpy(p_buff+1, g_extcmd.p_device_name, g_extcmd.device_name_len);
    *p_blen = g_extcmd.device_name_len + 1;
    err_code = BZ_SUCCESS;

end:
    return err_code;
}

static ret_code_t ext_cmd0A_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    if (*p_blen < 1) {
        BREEZE_ERR("Err:invalid DS update rsp buffer size");
        err_code = BZ_ENOMEM;
        goto end;
    }

    if(dlen != BZ_DEV_DEVICE_SECRET_LEN){
        BREEZE_ERR("Err:invalid DS update len");
        err_code = BZ_ENOMEM;
        goto end;
    }

    if(ext_get_device_secret() == true){
        p_buff[0] = 0x01;
    } else if(ext_set_device_secret(p_data, dlen) == true){
        auth_secret_update_post_process((uint8_t*)p_data, dlen);
        p_buff[0] = 0x00;
    } else{
        p_buff[0] = 0x02;
    }

    *p_blen = 1;

end:
    return err_code;

}
#endif

static SHM_DATA ret_code_t ext_cmd0B_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    uint8_t idx = 0, tlvtype, tlvlen;
    uint8_t rsp[] = { 0x01, 0x01, 0x01 };
    uint8_t auth_version = 0;
    uint8_t *p_auth_code = NULL;
    uint8_t auth_code_len = 0;
    uint8_t *p_auth_secret = NULL;
    uint8_t auth_secret_len = 0;
    uint8_t has_ac = 0;
    uint8_t auth_event = 0;

    if (dlen < 2) {
        err_code = BZ_EDATASIZE;
        goto end;
    }

    if (*p_blen < sizeof(rsp)) {
        err_code = BZ_ENOMEM;
        goto end;
    }

    g_auth_kv_val_len = sizeof(g_auth_kv_val);
    if (os_kv_get(BZ_AUTH_CODE_KV_PREFIX, g_auth_kv_val, &g_auth_kv_val_len) != 0) {
        BREEZE_DEBUG("no auth key get, AC can be add");
    } else {
        has_ac = 1;
        // Found AC, need manager checkin to modify it
        if (g_core.admin_checkin == 0) {
            err_code = BZ_ERROR_AC_AS_NO_PERMIT;
            BREEZE_DEBUG("no admin modify AC");
            goto end;
        } else {
            BREEZE_DEBUG("AC can be update");
        }
    }

    while (idx < dlen) {
        tlvtype = p_data[idx++];
        tlvlen  = p_data[idx++];

        switch (tlvtype) {
            case BZ_AUTH_INFO_AUTH_CODE:
                if (tlvlen > 16) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                p_auth_code = (uint8_t*)(p_data + idx);
                auth_code_len = tlvlen;
                BREEZE_VERBOSE("AC:%.*s", auth_code_len, p_auth_code);
                break;
            case BZ_AUTH_INFO_AUTH_SECRET:
                if (tlvlen < 1 || tlvlen > 32) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                p_auth_secret = (uint8_t*)(p_data + idx);
                auth_secret_len = tlvlen;
                BREEZE_VERBOSE("AS:%.*s", auth_secret_len, p_auth_secret);
                break;
            case BZ_AUTH_INFO_PROTOCOL_VER:
                if (tlvlen != 1) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                auth_version = p_data[idx];
                break;
            default:
                BREEZE_DEBUG("unknown AC/AS type");
                break;
        }

        idx += tlvlen;
    }

    g_auth_kv_val_len = 0;
    if (auth_code_len == 0) {
        // need clear AC & AS
        if (has_ac) {
            if (os_kv_del(BZ_AUTH_CODE_KV_PREFIX) != 0) {
                BREEZE_ERR("AC&AS clear failed");
                err_code = BZ_ERROR_AC_AS_DELETE;
            } else {
                auth_event = BZ_AC_AS_DELETE;
            }
        }
    } else {
        // need Add or Update AC & AS
        g_auth_kv_val[g_auth_kv_val_len++] = auth_code_len;
        memcpy(g_auth_kv_val + g_auth_kv_val_len, p_auth_code, auth_code_len);
        g_auth_kv_val_len += auth_code_len;
        if (auth_secret_len) {
            g_auth_kv_val[g_auth_kv_val_len++] = auth_secret_len;
            memcpy(g_auth_kv_val + g_auth_kv_val_len, p_auth_secret, auth_secret_len);
            g_auth_kv_val_len += auth_secret_len;
        } else {
            // do nothing, no auth secret found
        }
        g_auth_need_set = 1;
        
        if (has_ac) {
            auth_event = BZ_AC_AS_UPDATE;
        } else {
            auth_event = BZ_AC_AS_ADD;
        }
    }

end:
    if (err_code != BZ_SUCCESS) {
        rsp[2] = 2; /* set failure code */
    } else {
        // notify to upper layer
        core_event_notify(BZ_EVENT_AC_AS, &auth_event, sizeof(auth_event));
    }

    /* rsp */
    memcpy(p_buff, rsp, sizeof(rsp));
    *p_blen = sizeof(rsp);
    (void)auth_version;
    return err_code;
}

extern void utils_hmac_sha1_raw(const char *msg, int msg_len, char *digest, const char *key, int key_len);
extern void utils_hmac_sha1_base64(const char *msg, int msg_len, const char *key, int key_len, char *digest, int *digest_len);
static ret_code_t ext_cmd0C_rsp(uint8_t *p_buff, uint8_t *p_blen, const uint8_t *p_data, uint8_t dlen)
{
    ret_code_t err_code = BZ_SUCCESS;
    uint8_t idx = 0, tlvtype, tlvlen;
    uint8_t rsp[] = { 0x01, 0x01, 0x01 };
    uint8_t sign_version = 0;
    uint8_t access_key[17] = {0};
    uint8_t access_key_len = 0;
    uint8_t access_random[17] = {0};
    uint8_t access_random_len = 0;
    uint8_t auth_sign[20] = {0};
    uint8_t auth_sign_len = 0;
    uint8_t access_token[30];
    int access_token_len = sizeof(access_token);
    uint8_t auth_kv_data[16 + 32 + 2] = {0};
    int auth_kv_len = sizeof(auth_kv_data);
    uint8_t auth_code[17] = {0};
    uint8_t auth_code_len = 0;
    uint8_t auth_sec[33] = {0};
    uint8_t auth_sec_len = 0;
    char sign_out[20];
    uint8_t sign_event = 0;

    if (dlen < 2) {
        err_code = BZ_EDATASIZE;
        goto end;
    }

    if (*p_blen < sizeof(rsp)) {
        err_code = BZ_ENOMEM;
        goto end;
    }

    if (os_kv_get(BZ_AUTH_CODE_KV_PREFIX, auth_kv_data, &auth_kv_len) != 0) {
        BREEZE_DEBUG("no auth key get");
        g_core.admin_checkin = 1;
        g_core.guest_checkin = 1;
        sign_event = BZ_AUTH_SIGN_NO_CHECK_PASS;
        goto end;
    }
    if (auth_kv_len > 0) {
        auth_code_len = auth_kv_data[0];
        if (auth_kv_len < auth_code_len + 1) {
            BREEZE_ERR("auth kv len err(%d < %d)", auth_kv_len, auth_code_len + 1);
            err_code = BZ_ERROR_AC_AS_DATA_LEN;
            goto end;
        }
        memcpy(auth_code, auth_kv_data + 1, auth_code_len);
        if (auth_kv_len > (1 + auth_code_len)) {
            auth_sec_len = auth_kv_data[1 + auth_code_len];
            memcpy(auth_sec, auth_kv_data + 1 + auth_code_len + 1, auth_sec_len);
        } else {
            auth_sec_len = g_extcmd.device_secret_len;
            memcpy(auth_sec, g_extcmd.p_device_secret, auth_sec_len);
        }
    } else {
        BREEZE_ERR("auth kv len err(%d)", auth_kv_len);
        err_code = BZ_ERROR_AC_AS_DATA_LEN;
        goto end;
    }

    while (idx < dlen) {
        tlvtype = p_data[idx++];
        tlvlen  = p_data[idx++];

        switch (tlvtype) {
            case BZ_AUTH_SIGN_ACCESS_KEY:
                if (tlvlen < 1 || tlvlen > 16) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                access_key_len = tlvlen;
                memcpy(access_key, &p_data[idx], tlvlen);
                BREEZE_VERBOSE("AK:%.*s", access_key_len, access_key);
                break;
            case BZ_AUTH_SIGN_RANDOM:
                if (tlvlen < 1 || tlvlen > 16) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                access_random_len = tlvlen;
                memcpy(access_random, &p_data[idx], tlvlen);
                BREEZE_VERBOSE("Rand:");
                hex_byte_dump_verbose(access_random, access_random_len, 24);
                break;
            case BZ_AUTH_SIGN_AUTH_SIGN:
                if (tlvlen < 1 || tlvlen > 32) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                auth_sign_len = tlvlen;
                memcpy(auth_sign, &p_data[idx], tlvlen);
                BREEZE_VERBOSE("ASign:");
                hex_byte_dump_verbose(auth_sign, auth_sign_len, 24);
                break;
            case BZ_AUTH_SIGN_PROTOCOL_VER:
                if (tlvlen != 1) {
                    err_code = BZ_EINVALIDTLV;
                    goto end;
                }
                sign_version = p_data[idx];
                break;
            default:
                BREEZE_DEBUG("unknown AuthSign type");
                break;
        }

        idx += tlvlen;
    }
    if (!access_key_len || !access_random_len || !auth_sign_len) {
        err_code = BZ_ERROR_AUTH_DATA;
        goto end;
    }
    // check AccessKey
    if (memcmp(auth_code, access_key, auth_code_len) == 0) {
        // check AuthSign
        utils_hmac_sha1_base64((const char *)access_key, (int)access_key_len, 
                               (const char *)auth_sec, (int)auth_sec_len, 
                               (char *)access_token, &access_token_len);
        BREEZE_VERBOSE("AT:%.*s", access_token_len, access_token);
        utils_hmac_sha1_raw((const char *)access_random, (int)access_random_len, 
                            (char *)sign_out, 
                            (const char *)access_token, (int)access_token_len);
        BREEZE_VERBOSE("sign_out:");
        hex_byte_dump_verbose((uint8_t*)sign_out, sizeof(sign_out), 24);
        if (memcmp(sign_out, auth_sign, auth_sign_len) == 0) {
            BREEZE_DEBUG("AC auth ok");
            if (access_key[access_key_len - 1] == '0') {
                g_core.admin_checkin = 1;
            } else if (access_key[access_key_len - 1] == '1') {
                g_core.guest_checkin = 1;
            }
            sign_event = BZ_AUTH_SIGN_CHECK_PASS;
        } else {
            BREEZE_DEBUG("AC auth fail");
            err_code = BZ_ERROR_AUTH_SIGN;
        }
    } else {
        BREEZE_DEBUG("AC auth fail");
        err_code = BZ_ERROR_AUTH_SIGN;
    }

end:
    if (err_code != BZ_SUCCESS) {
        rsp[2] = 2; /* set failure code */
    } else {
        // notify to upper layer
        core_event_notify(BZ_EVENT_AUTH_SIGN, &sign_event, sizeof(sign_event));
    }

    /* rsp */
    memcpy(p_buff, rsp, sizeof(rsp));
    *p_blen = sizeof(rsp);
    (void)sign_version;
    return err_code;
}

static void get_os_info(void)
{
    uint8_t suffix_len = 0;    
#ifdef BUILD_AOS    
    uint8_t chip_code[4] = { 0 };
    uint8_t chip_id_str[8] = { 0 };
    const char *aostype = "AOS";

    char t_os_info[20] = { 0 };

    strcpy(t_os_info, aos_version_get());
    char *m_os_type = strtok_r(t_os_info, "-", &pSavedPtr);
    if (strcmp(aostype, m_os_type) == 0) {
        m_os_type = strtok_r(NULL, "-", &pSavedPtr);
        m_os_type = strtok_r(NULL, "-", &pSavedPtr);
        strcat(m_os_type, ":");
        BREEZE_TRACE("AOS version %s(%d)\n", m_os_type, strlen(m_os_type));

        suffix_len = strlen(m_os_type);
        memcpy(g_extcmd.tlv_01_rsp, m_os_type, suffix_len);
        chip_code_st *p_chip_code_obj = get_chip_code(MCU_FAMILY);
        if (p_chip_code_obj != NULL) {
            chip_code[0] = (uint8_t)(p_chip_code_obj->vendor >> 8);
            chip_code[1] = (uint8_t)p_chip_code_obj->vendor;
            chip_code[2] = (uint8_t)(p_chip_code_obj->id >> 8);
            chip_code[3] = (uint8_t)p_chip_code_obj->id;
        }

        hex2string(chip_code, sizeof(chip_code), chip_id_str);
        memcpy(g_extcmd.tlv_01_rsp + suffix_len, chip_id_str, sizeof(chip_id_str));
        suffix_len += sizeof(chip_id_str);
        memcpy(g_extcmd.tlv_01_rsp + suffix_len, m_sdk_version, sizeof(m_sdk_version) - 1);
        suffix_len += sizeof(m_sdk_version) - 1;
        g_extcmd.tlv_01_rsp[suffix_len] = '\0';
        strcat(g_extcmd.tlv_01_rsp, ":1");
        suffix_len = strlen(g_extcmd.tlv_01_rsp);
    }
#else
    memcpy(g_extcmd.tlv_01_rsp, "NON-AOS", strlen("NON-AOS"));
    g_extcmd.tlv_01_rsp[suffix_len] = '\0';
    suffix_len = strlen("NON-AOS");
#endif
    g_extcmd.tlv_01_rsp_len = suffix_len;
}

SHM_DATA ret_code_t extcmd_init(ali_init_t const *p_init, tx_func_t tx_func)
{
    memset(&g_extcmd, 0, sizeof(extcmd_t));
    get_os_info();

    if ((p_init->product_key.p_data != NULL) && (p_init->product_key.length > 0)) {
        g_extcmd.product_key_len = g_core.product_key_len;
        g_extcmd.p_product_key = g_core.product_key;
    }
    if ((p_init->device_name.p_data != NULL) && (p_init->device_name.length > 0)) {
        g_extcmd.device_name_len = g_core.device_name_len;
        g_extcmd.p_device_name = g_core.device_name;
    }
    if ((p_init->device_secret.p_data != NULL) && (p_init->device_secret.length > 0)) {
        g_extcmd.device_secret_len = g_core.device_secret_len;
        g_extcmd.p_device_secret = g_core.device_secret;
    }

    g_extcmd.tx_func = tx_func;
    g_extcmd.model_id = p_init->model_id;
    return BZ_SUCCESS;
}

SHM_DATA void extcmd_rx_command(uint8_t cmd, uint8_t *p_data, uint16_t length)
{
    if (length == 0 || cmd != BZ_CMD_EXT_DOWN) {
        return;
    }

    uint8_t *p_tx_buff = g_extcmd.tx_buff;
    uint8_t tx_buff_avail = sizeof(g_extcmd.tx_buff);
    uint8_t tx_buff_size;
    uint8_t tlv_mask, tlv_masked = 0;
    uint8_t tlv_type, tlv_len;

    uint32_t err_code = BZ_SUCCESS;

    g_auth_need_set = 0;

//    printf("\n\nextcmd_rx_command\n");
    hex_byte_dump_debug(p_data, length, 24);

#if BZ_ENABLE_AUTH
    if (!auth_is_authdone()) {
        err_code = BZ_EINVALIDSTATE;
    }
#endif

    while (length > 0 && err_code == BZ_SUCCESS) {
        if (length >= 2) {
            /* get TLV type. */
            tlv_type = *p_data++;
            /* get TLV length. */
            tlv_len = *p_data++;

            length -= 2;
        } else {
            err_code = BZ_EINVALIDLEN;
            break;
        }

        /* each TLV type should not get repeated. */
        tlv_mask = (1 << tlv_type);
        if ((tlv_mask & tlv_masked) != 0) {
            printf("\n\nif ((tlv_mask & tlv_masked) != 0)\n");
            err_code = BZ_EINVALIDDATA;
            break;
        }
        tlv_masked |= tlv_mask;

        /* check that TLV length does not exceed input data boundary. */
        if (tlv_len > length) {
            printf("\n\nif (tlv_len > length)!= 0)\n");
            err_code = BZ_EDATASIZE;
            break;
        }

        if (tx_buff_avail < 2) {
            err_code = BZ_ENOMEM;
            break;
        }

        uint32_t n, n_max = sizeof(m_tlv_type_handler_table) / sizeof(ext_tlv_type_handler_t);
        for (n = 0; n < n_max; n++) {
            if (m_tlv_type_handler_table[n].tlv_type == tlv_type) {
                tx_buff_size = tx_buff_avail - 2;
                err_code = m_tlv_type_handler_table[n].handler(p_tx_buff + 2, &tx_buff_size, p_data, tlv_len);
                break;
            }
        }
        if (n >= n_max) {
            printf("\n\nif (n >= n_max)!= 0)\n");
            err_code = BZ_EINVALIDDATA;
        }

        if (err_code == BZ_SUCCESS) {
            *(p_tx_buff + 0) = tlv_type;
            *(p_tx_buff + 1) = tx_buff_size;

            p_tx_buff += (2 + tx_buff_size);
            tx_buff_avail -= (2 + tx_buff_size);

            p_data += tlv_len;
            length -= tlv_len;
        }
    }

    if (err_code == BZ_SUCCESS) {
        err_code = g_extcmd.tx_func(BZ_CMD_EXT_UP, g_extcmd.tx_buff, sizeof(g_extcmd.tx_buff) - tx_buff_avail);
        if (err_code == BZ_SUCCESS) {
            if (g_auth_need_set && g_auth_kv_val_len) {
                if (os_kv_set(BZ_AUTH_CODE_KV_PREFIX, g_auth_kv_val, g_auth_kv_val_len, 1) != 0) {
                    BREEZE_ERR("AC&AS set failed");
                    err_code = BZ_ERROR_AC_AS_STORE;
                }
            }
        }
    } else {
        err_code = g_extcmd.tx_func(BZ_CMD_ERR, NULL, 0);
    }

    if (err_code != BZ_SUCCESS) {
        core_handle_err(ALI_ERROR_SRC_EXT_SEND_RSP, err_code);
    }
}
