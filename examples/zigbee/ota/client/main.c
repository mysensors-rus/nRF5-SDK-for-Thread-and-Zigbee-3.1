/**
 * Copyright (c) 2018 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup zigbee_examples_ota_client main.c
 * @{
 * @ingroup zigbee_examples
 * @brief OTA Client example
 */

#include "zboss_api.h"
#include "zboss_api_addons.h"
#include "zb_mem_config_med.h"
#include "zb_error_handler.h"
#include "zigbee_helpers.h"
#include "zigbee_dfu_transport.h"

#include "app_timer.h"
#include "app_scheduler.h"

#include "boards.h"

#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ota_upgrade_client.h"

#if ! defined ZB_ROUTER_ROLE
#error define ZB_ROUTER_ROLE to compile zr tests
#endif

#define MAX_CHILDREN                      10                                    /**< The maximum amount of connected devices. Setting this value to 0 disables association to this device.  */
#define IEEE_CHANNEL_MASK                 (1l << ZIGBEE_CHANNEL)                /**< Scan only one, predefined channel to find the coordinator. */
#define OTA_ENDPOINT                      10                                    /**< Endpoint for the OTA Client functionality */
#define ERASE_PERSISTENT_CONFIG           ZB_FALSE                              /**< Erase NVRAM to reset the network parameters after device reboot or power-off. */
#define ZIGBEE_NETWORK_STATE_LED          BSP_BOARD_LED_2                       /**< LED indicating that OTA Client successfully joined ZigBee network. */
#define OTA_ACTIVITY                      BSP_BOARD_LED_3                       /**< LED OTA Client Activity */

#define OTA_UPGRADE_TEST_FILE_VERSION     0x01010101
#define OTA_UPGRADE_TEST_MANUFACTURER     123
#define OTA_UPGRADE_TEST_IMAGE_TYPE       321
#define OTA_UPGRADE_TEST_DATA_SIZE        BACKGROUND_DFU_DEFAULT_BLOCK_SIZE
#define HARDWARE_VERSION                  0x0101


typedef struct
{
    zb_uint8_t zcl_version;
    zb_uint8_t power_source;
} ota_client_basic_attr_t;

typedef struct
{
    zb_ieee_addr_t upgrade_server;
    zb_uint32_t    file_offset;
    zb_uint32_t    file_version;
    zb_uint16_t    stack_version;
    zb_uint32_t    downloaded_file_ver;
    zb_uint32_t    downloaded_stack_ver;
    zb_uint8_t     image_status;
    zb_uint16_t    manufacturer;
    zb_uint16_t    image_type;
    zb_uint16_t    min_block_reque;
    zb_uint16_t    image_stamp;
    zb_uint16_t    server_addr;
    zb_uint8_t     server_ep;
} ota_client_ota_upgrade_attr_t;

typedef struct
{
    ota_client_basic_attr_t       basic_attr;
    ota_client_ota_upgrade_attr_t ota_attr;
} ota_client_ctx_t;

/* Create an instance of app_timer for OTA server periodical discovery. */
APP_TIMER_DEF(m_ota_server_discovery_timer);
static zb_zcl_ota_upgrade_client_periodical_discovery_ctx_t m_discovery_ctx;

/******************* Declare attributes ************************/
static ota_client_ctx_t m_dev_ctx;

/* Basic cluster attributes data */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list,
                                 &m_dev_ctx.basic_attr.zcl_version,
                                 &m_dev_ctx.basic_attr.power_source);

/* OTA cluster attributes data */
ZB_ZCL_DECLARE_OTA_UPGRADE_ATTRIB_LIST(ota_upgrade_attr_list,
                                       m_dev_ctx.ota_attr.upgrade_server,
                                       &m_dev_ctx.ota_attr.file_offset,
                                       &m_dev_ctx.ota_attr.file_version,
                                       &m_dev_ctx.ota_attr.stack_version,
                                       &m_dev_ctx.ota_attr.downloaded_file_ver,
                                       &m_dev_ctx.ota_attr.downloaded_stack_ver,
                                       &m_dev_ctx.ota_attr.image_status,
                                       &m_dev_ctx.ota_attr.manufacturer,
                                       &m_dev_ctx.ota_attr.image_type,
                                       &m_dev_ctx.ota_attr.min_block_reque,
                                       &m_dev_ctx.ota_attr.image_stamp,
                                       &m_dev_ctx.ota_attr.server_addr,
                                       &m_dev_ctx.ota_attr.server_ep,
                                       HARDWARE_VERSION,
                                       OTA_UPGRADE_TEST_DATA_SIZE,
                                       ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF);

/********************* Declare device **************************/

ZB_HA_DECLARE_OTA_UPGRADE_CLIENT_CLUSTER_LIST( ota_upgrade_client_clusters,
          basic_attr_list, ota_upgrade_attr_list);

ZB_HA_DECLARE_OTA_UPGRADE_CLIENT_EP(ota_upgrade_client_ep, OTA_ENDPOINT, ota_upgrade_client_clusters);

ZB_HA_DECLARE_OTA_UPGRADE_CLIENT_CTX(ota_upgrade_client_ctx, ota_upgrade_client_ep);


/******************************************************************/

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    ret_code_t err_code;

    // Initialize timer module.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing all clusters attributes.
 */
static void ota_client_attr_init(void)
{
    /* Basic cluster attributes data */
    m_dev_ctx.basic_attr.zcl_version  = ZB_ZCL_VERSION;
    m_dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;

    /* OTA cluster attributes data */
    zb_ieee_addr_t addr = ZB_ZCL_OTA_UPGRADE_SERVER_DEF_VALUE;
    ZB_MEMCPY(m_dev_ctx.ota_attr.upgrade_server, addr, sizeof(zb_ieee_addr_t));
    m_dev_ctx.ota_attr.file_offset = ZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEF_VALUE;
    m_dev_ctx.ota_attr.file_version = OTA_UPGRADE_TEST_FILE_VERSION;
    m_dev_ctx.ota_attr.stack_version = ZB_ZCL_OTA_UPGRADE_FILE_HEADER_STACK_PRO;
    m_dev_ctx.ota_attr.downloaded_file_ver  = ZB_ZCL_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_DEF_VALUE;
    m_dev_ctx.ota_attr.downloaded_stack_ver = ZB_ZCL_OTA_UPGRADE_DOWNLOADED_STACK_DEF_VALUE;
    m_dev_ctx.ota_attr.image_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DEF_VALUE;
    m_dev_ctx.ota_attr.manufacturer = OTA_UPGRADE_TEST_MANUFACTURER;
    m_dev_ctx.ota_attr.image_type = OTA_UPGRADE_TEST_IMAGE_TYPE;
    m_dev_ctx.ota_attr.min_block_reque = 0;
    m_dev_ctx.ota_attr.image_stamp = ZB_ZCL_OTA_UPGRADE_IMAGE_STAMP_MIN_VALUE;
}

/** @brief Code for rebooting the chip
 *
 *  @param param Unused
 */
static void reboot_application(zb_uint8_t param)
{
    UNUSED_VARIABLE(param);
    NRF_LOG_FINAL_FLUSH();

    #if NRF_MODULE_ENABLED(NRF_LOG_BACKEND_RTT)
    // To allow the buffer to be flushed by the host.
    nrf_delay_ms(100);
    #endif

    NVIC_SystemReset();
}

static zb_void_t zcl_device_cb(zb_uint8_t param)
{
    zb_buf_t * p_buffer = ZB_BUF_FROM_REF(param);
    zb_zcl_device_callback_param_t * p_device_cb_param = ZB_GET_BUF_PARAM(p_buffer, zb_zcl_device_callback_param_t);

    p_device_cb_param->status = RET_OK;
    switch (p_device_cb_param->device_cb_id)
    {
        case ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID:
        {
            zb_zcl_ota_upgrade_value_param_t * p_ota_upgrade_value = &(p_device_cb_param->cb_param.ota_value_param);

            switch (p_ota_upgrade_value->upgrade_status)
            {
                case ZB_ZCL_OTA_UPGRADE_STATUS_START:
                    /* Check if OTA client is in the middle of image download.
                        If so, silently ignore the second QueryNextImageResponse packet from OTA server. */
                    if (zb_zcl_ota_upgrade_get_ota_status(p_device_cb_param->endpoint) != ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_NORMAL)
                    {
                        p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_BUSY;
                    }

                    /* Check if we're not downgrading.
                       If we do, let's politely say no since we do not support that. */
                    else if (p_ota_upgrade_value->upgrade.start.file_version > m_dev_ctx.ota_attr.file_version)
                    {
                        p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                    }
                    else
                    {
                        p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ABORT;
                    }
                    break;

                case ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
                    /* Process image block. */
                    p_ota_upgrade_value->upgrade_status = zb_process_chunk(p_ota_upgrade_value, param);
                    bsp_board_led_invert(OTA_ACTIVITY);
                    break;

                case ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
                    p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                    break;

                case ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
                    p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                    break;

                case ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
                    /* It is time to upgrade FW. */
                    /* We use callback so the stack can have time to i.e. send response etc */
                    UNUSED_RETURN_VALUE(ZB_SCHEDULE_CALLBACK(reboot_application, 0));
                    break;

                case ZB_ZCL_OTA_UPGRADE_STATUS_ABORT:
                    NRF_LOG_INFO("Zigbee DFU Aborted");
                    p_ota_upgrade_value->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ABORT;
                    zb_abort_dfu();
                    break;

                default:
                    break;
            }
            /* No need to free the buffer - stack handles that if needed */
        }
            break;
        default:
            break;
    }
}

void zboss_signal_handler(zb_uint8_t param)
{
    zb_zdo_app_signal_hdr_t  * p_sg_p = NULL;
    zb_zdo_app_signal_type_t   sig    = zb_get_app_signal(param, &p_sg_p);
    zb_ret_t                   status = ZB_GET_APP_SIGNAL_STATUS(param);
    zb_bool_t                  comm_status;

    switch(sig)
    {
        case ZB_ZDO_SIGNAL_DEFAULT_START:
        case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (status == RET_OK)
            {
                NRF_LOG_INFO("Joined network successfully");
                bsp_board_led_on(ZIGBEE_NETWORK_STATE_LED);

                ret_code_t err_code = zb_zcl_ota_upgrade_client_with_periodical_discovery_start(&m_discovery_ctx);
                APP_ERROR_CHECK(err_code);
            }
            else
            {
                NRF_LOG_ERROR("Failed to join network. Status: %d", status);
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);
                comm_status = bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
                ZB_COMM_STATUS_CHECK(comm_status);
            }
            break;

        case ZB_ZDO_SIGNAL_LEAVE:
            if (status == RET_OK)
            {
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);

                zb_zdo_signal_leave_params_t * p_leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_leave_params_t);
                NRF_LOG_INFO("Network left. Leave type: %d", p_leave_params->leave_type);
            }
            else
            {
                NRF_LOG_ERROR("Unable to leave network. Status: %d", status);
            }
            break;

        case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
            break;

        default:
            /* Unhandled signal. For more information see: zb_zdo_app_signal_type_e and zb_ret_e */
            NRF_LOG_INFO("Unhandled signal %d. Status: %d", sig, status);
            break;
    }

    if (param)
    {
        ZB_FREE_BUF_BY_REF(param);
    }
}

int main(void)
{
    zb_ret_t       zb_err_code;
    zb_ieee_addr_t ieee_addr;
    ret_code_t     err_code;

    /* Initialize timers, logging and LEDs */
    timers_init();
    log_init();
    bsp_board_init(BSP_INIT_LEDS);

    /* Set ZigBee stack logging level and traffic dump subsystem. */
    ZB_SET_TRACE_LEVEL(ZIGBEE_TRACE_LEVEL);
    ZB_SET_TRACE_MASK(ZIGBEE_TRACE_MASK);
    ZB_SET_TRAF_DUMP_OFF();

    /* Initialize the attributes */
    ota_client_attr_init();

    /* Initialize the Zigbee DFU Transport */
    zb_dfu_init(OTA_ENDPOINT);

    /* Initialize ZigBee stack. */
    ZB_INIT("ota_client");

    /* Set device address to the value read from FICR registers. */
    zb_osif_get_ieee_eui64(ieee_addr);
    zb_set_long_address(ieee_addr);

    /* Set static long IEEE address. */
    zb_set_network_router_role(IEEE_CHANNEL_MASK);
    zb_set_max_children(MAX_CHILDREN);
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));

    /* Register OTA Client device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&ota_upgrade_client_ctx);

    /* Register callback for handling ZCL commands. */
    ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

    /* Initialize Periodic OTA server discovery */
    err_code = zb_zcl_ota_upgrade_client_with_periodical_discovery_init(&m_discovery_ctx, &m_ota_server_discovery_timer, OTA_ENDPOINT);
    APP_ERROR_CHECK(err_code);

    zb_err_code = zboss_start();
    ZB_ERROR_CHECK(zb_err_code);
    while(1)
    {
        zboss_main_loop_iteration();
        app_sched_execute();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    }
}
