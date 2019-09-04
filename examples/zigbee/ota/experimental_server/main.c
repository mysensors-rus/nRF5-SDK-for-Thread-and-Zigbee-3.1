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
 * @defgroup zigbee_examples_ota_server main.c
 * @{
 * @ingroup zigbee_examples
 * @brief OTA Server example to be used with the nrfutil dfu zigbee command or as a standalone app.
 *
 * @details The application assumes that a correct OTA Upgrade file is located at the UPGRADE_IMAGE_OFFSET
 *          location. After the start the server try to commission itself to the Zigbee Network and after
 *          5 seconds starts disseminating the update.
 *          When compiled with ZIGBEE_OTA_SERVER_USE_CLI flag, the CLI interface is added - it is needed
 *          when using the server with the nrfutil (the nrfutil then explicitly uses the 'zdo channel' and
 *          'bdb start' commands to begin the commissioning).
 */

#include "zboss_api.h"
#include "zb_mem_config_med.h"
#include "zb_error_handler.h"
#include "zigbee_helpers.h"

#include "app_timer.h"
#include "nrf_drv_clock.h"
#include "app_scheduler.h"
#include "nrf_dfu_utils.h"
#include "nrf_dfu_transport.h"
#include "nrf_bootloader_info.h"
#include "nrf_dfu_settings.h"
#include "nrf_dfu_req_handler.h"
#include "nrf_dfu_validation.h"
#include "nrf_dfu_ver_validation.h"
#include "nrf_drv_power.h"

#include "boards.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ota_upgrade_server.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_dfu_ble.h"
#endif

#ifdef SOFTDEVICE_PRESENT
#include "nrf_dfu_ble.h"
#endif

#define SCHED_QUEUE_SIZE      64                                                /**< Maximum number of events in the scheduler queue. */
#define SCHED_EVENT_DATA_SIZE MAX((sizeof(nrf_dfu_request_t)), APP_TIMER_SCHED_EVENT_DATA_SIZE) /**< Maximum app_scheduler event size. */

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
#include "zigbee_cli.h"
#endif

#if ! defined ZB_ROUTER_ROLE
#error define ZB_ROUTER_ROLE to compile OTA Server
#endif

#define MAX_CHILDREN                      10                                    /**< The maximum amount of connected devices. Setting this value to 0 disables association to this device.  */
#define IEEE_CHANNEL_MASK                 (1l << ZIGBEE_CHANNEL)                /**< Scan only one, predefined channel to find the coordinator. */
#ifdef SOFTDEVICE_PRESENT
#define ERASE_PERSISTENT_CONFIG           ZB_FALSE                              /**< Do no erase NVRAM. Keep the network parameters after device reboot or power-off. */
#else
#define ERASE_PERSISTENT_CONFIG           ZB_TRUE                               /**< Erase NVRAM to reset the network parameters after device reboot or power-off. */
#endif
#define ZIGBEE_NETWORK_STATE_LED          BSP_BOARD_LED_2                       /**< LED indicating that OTA Server successfully joined ZigBee network. */
#define OTA_IMAGE_PRESENT_LED             BSP_BOARD_LED_3                       /**< LED indicating that a correct OTA Upgrade file is present. */
#define BLE_OTA_ACTIVITY_LED              BSP_BOARD_LED_3                       /**< LED indicating that a new image is transferred through BLE. */
#define ZIGBEE_OTA_IMAGE_NOTIFY_DELAY     ZB_MILLISECONDS_TO_BEACON_INTERVAL(5 * 1000) /**< Additional delay, in beacon intervals between device startup, if correct image was found inside flash memory, and sending Zigbee Image Notify message. */

#define OTA_ENDPOINT                      5
#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
#define CLI_AGENT_ENDPOINT                64                                    /**< Source endpoint used to control light bulb. */
#endif

#ifndef SOFTDEVICE_PRESENT
#define UPGRADE_IMAGE_OFFSET              0x80000                               /**< The address inside flash, were Zigbee image is stored. This value has to be aligned with nrfutil constant (OTA_UPDATE_OFFSET inside OTAFlasher class). */
#else
#define UPGRADE_IMAGE_OFFSET              nrf_dfu_bank1_start_addr()            /**< The address inside flash, were Zigbee image is stored. By default BLE DFU stores images inside BANK1, which address depends on the server application size */
#endif

#define NUMBER_OF_UPGRADE_IMAGES          1
#define OTA_UPGRADE_TEST_CURRENT_TIME     0x00000000                            /**< Server does not support Time cluster, use OTA_UPGRADE_TEST_UPGRADE_TIME as delay value. */
#ifndef OTA_UPGRADE_TEST_UPGRADE_TIME
#define OTA_UPGRADE_TEST_UPGRADE_TIME     0x00000001                            /**< If OTA_UPGRADE_TEST_CURRENT_TIME set to zero, use this value as firmware upgrade delay in seconds. */
#endif


typedef struct
{
    zb_uint8_t zcl_version;
    zb_uint8_t power_source;
} ota_server_basic_attr_t;

typedef struct
{
    zb_uint8_t  query_jitter;
    zb_uint32_t current_time;
} ota_server_ota_upgrade_attr_t;

typedef struct
{
    ota_server_basic_attr_t       basic_attr;
    ota_server_ota_upgrade_attr_t ota_attr;
} ota_server_ctx_t;

typedef ZB_PACKED_PRE struct ota_upgrade_test_file_s
{
  zb_zcl_ota_upgrade_file_header_t head;
  zb_uint8_t data[1];
} ZB_PACKED_STRUCT ota_upgrade_test_file_t;

static zb_bool_t                 m_stack_started = ZB_FALSE;
static ota_upgrade_test_file_t * mp_ota_file = NULL;
#ifdef SOFTDEVICE_PRESENT
static bool m_ota_file_inserted  = false;
static bool m_external_app_valid = false;
#endif
/******************* Declare attributes ************************/

static ota_server_ctx_t m_dev_ctx;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list,
                                 &m_dev_ctx.basic_attr.zcl_version,
                                 &m_dev_ctx.basic_attr.power_source);

ZB_ZCL_DECLARE_OTA_UPGRADE_ATTRIB_LIST_SERVER(ota_upgrade_attr_list,
                                              &m_dev_ctx.ota_attr.query_jitter,
                                              &m_dev_ctx.ota_attr.current_time,
                                              NUMBER_OF_UPGRADE_IMAGES);

/********************* Declare device **************************/

ZB_HA_DECLARE_OTA_UPGRADE_SERVER_CLUSTER_LIST(ota_upgrade_server_clusters,
                                              basic_attr_list,
                                              ota_upgrade_attr_list);

ZB_HA_DECLARE_OTA_UPGRADE_SERVER_EP(ota_upgrade_server_ep, OTA_ENDPOINT, ota_upgrade_server_clusters);

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
static zb_uint16_t m_attr_identify_time = 0;

/* Declare attribute list for Identify cluster. */
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &m_attr_identify_time);

/* Declare cluster list for CLI Agent device. */
/* Only clusters Identify and Basic have attributes. */
ZB_HA_DECLARE_CONFIGURATION_TOOL_CLUSTER_LIST(cli_agent_clusters,
                                              basic_attr_list,
                                              identify_attr_list);

/* Declare endpoint for CLI Agent device. */
ZB_HA_DECLARE_CONFIGURATION_TOOL_EP(cli_agent_ep,
                                    ZIGBEE_CLI_ENDPOINT,
                                    cli_agent_clusters);

/* Declare application's device context (list of registered endpoints) for CLI Agent device. */
ZB_HA_DECLARE_CONFIGURATION_TOOL_CTX(cli_agent_ctx, cli_agent_ep);

ZBOSS_DECLARE_DEVICE_CTX_2_EP(ota_upgrade_server_ctx, ota_upgrade_server_ep, cli_agent_ep);
#else /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */
ZB_HA_DECLARE_OTA_UPGRADE_SERVER_CTX(ota_upgrade_server_ctx, ota_upgrade_server_ep);
#endif /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */

#ifdef SOFTDEVICE_PRESENT
/* Forward declaration of functions controlling BLE DFU transport. */
uint32_t ble_dfu_transport_init(nrf_dfu_observer_t observer);
uint32_t ble_dfu_transport_close(nrf_dfu_transport_t const * p_exception);
#endif

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

/**@brief Function for initializing scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for initializing the Power Driver.
 */
static void power_driver_init(void)
{
    ret_code_t err_code;
    err_code = nrf_drv_power_init(NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing all clusters attributes.
 */
static void ota_server_attr_init(void)
{
    /* Basic cluster attributes data */
    m_dev_ctx.basic_attr.zcl_version  = ZB_ZCL_VERSION;
    m_dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;

    /* OTA cluster attributes data */
    m_dev_ctx.ota_attr.query_jitter = ZB_ZCL_OTA_UPGRADE_QUERY_JITTER_MAX_VALUE;
    m_dev_ctx.ota_attr.current_time = OTA_UPGRADE_TEST_CURRENT_TIME;
}

/**@brief This callback is called on next image block request
 *
 * @param index  file index
 * @param offset current offset of the file
 * @param size   block size
 */
static zb_uint8_t * next_data_ind_cb(zb_uint8_t index, zb_uint32_t offset, zb_uint8_t size)
{
    return ((zb_uint8_t *)mp_ota_file + offset);
}

/**@brief Function for checking if a new Zigbee image is present at given address.
 *
 * @params[in] p_ota_file  Pointer to the memory, where Zigbee OTA image starts.
 *
 * @returns true if a valid image is found, false otherwise.
 */
static bool ota_file_sanity_check(ota_upgrade_test_file_t * p_ota_file)
{
    if (p_ota_file->head.file_id != ZB_ZCL_OTA_UPGRADE_FILE_HEADER_FILE_ID)
    {
        bsp_board_led_off(OTA_IMAGE_PRESENT_LED);
        return false;
    }
    else
    {
        bsp_board_led_on(OTA_IMAGE_PRESENT_LED);
        return true;
    }
}

static zb_void_t insert_ota_file(zb_uint8_t param)
{
    zb_buf_t * buf = (zb_buf_t *)ZB_BUF_FROM_REF(param);
    /* The function assumes that at the UPGRADE_IMAGE_OFFSET address the correct OTA upgrade file can be found */
    ZB_ZCL_OTA_UPGRADE_INSERT_FILE(buf, OTA_ENDPOINT, 0, (zb_uint8_t *)(mp_ota_file), OTA_UPGRADE_TEST_UPGRADE_TIME);
#ifdef SOFTDEVICE_PRESENT
    m_ota_file_inserted = true;
#endif
}

#ifdef SOFTDEVICE_PRESENT
static zb_void_t remove_ota_file(zb_uint8_t param)
{
    if (!m_ota_file_inserted)
    {
        return;
    }

    zb_buf_t * buf = (zb_buf_t *)ZB_BUF_FROM_REF(param);
    ZB_ZCL_OTA_UPGRADE_REMOVE_FILE(buf, OTA_ENDPOINT, 0);
    m_ota_file_inserted = false;
}

/**@brief Function for reseting the OTA server. After update, the background DFU
 *        bootloader will apply the new firmware.
 *
 * @param[in]   param   Not used. Required by callback type definition.
 */
static zb_void_t reset_ota_server(zb_uint8_t param)
{
    if (param)
    {
        ZB_FREE_BUF_BY_REF(param);
    }

    NRF_LOG_FINAL_FLUSH();
    NVIC_SystemReset();
}

/**@brief Function for handling a self-upgrade DFU process.
 *
 * @details Cannot close transport directly form event context, as it is called
 *          directly from softdevice event and disabling softdevice (by closing
 *          BLE transport) in an softdevice-interrupt context is prohibited.
 **/
static void ble_transport_disable(void * p_event_data, uint16_t event_size)
{
    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);

    UNUSED_RETURN_VALUE(ble_dfu_transport_close(NULL));
}

/**@brief Function notifies certain events in DFU process. */
static void dfu_observer(nrf_dfu_evt_type_t event)
{
    static bool dfu_image_valid      = false;
    static bool ble_transport_active = false;
    zb_ret_t    zb_err_code;

    switch (event)
    {
        case NRF_DFU_EVT_TRANSPORT_DEACTIVATED:
            if (ota_file_sanity_check(mp_ota_file))
            {
                NRF_LOG_INFO("BLE peer successfully disconnected.");
            }
            else
            {
                NRF_LOG_INFO("BLE transport deactivated.");
                ble_transport_active = false;

                if (dfu_image_valid)
                {
                    NRF_LOG_INFO("New firmware for OTA server accepted. Reset device.");
                    UNUSED_RETURN_VALUE(ZB_SCHEDULE_CALLBACK(reset_ota_server, 0));
                }
            }
            break;

        case NRF_DFU_EVT_DFU_INITIALIZED:
            break;

        case NRF_DFU_EVT_TRANSPORT_ACTIVATED:
            ble_transport_active = true;
            break;

        case NRF_DFU_EVT_DFU_STARTED:
            NRF_LOG_INFO("BLE image transfer started.");
            NRF_LOG_INFO("Store new image at 0x%08x", mp_ota_file);

            // A new Zigbee image is going to be transferred over BLE and will overwrite current image contents.
            // Invalidate current image, so any ongoing Zigbee DFU process will be aborted by OTA server.
            zb_err_code = ZB_GET_OUT_BUF_DELAYED(remove_ota_file);
            ZB_ERROR_CHECK(zb_err_code);

            bsp_board_led_on(BLE_OTA_ACTIVITY_LED);
            dfu_image_valid = false;
            break;

        case NRF_DFU_EVT_OBJECT_RECEIVED:
            bsp_board_led_invert(BLE_OTA_ACTIVITY_LED);
            break;

        case NRF_DFU_EVT_DFU_FAILED:
        case NRF_DFU_EVT_DFU_ABORTED:
            NRF_LOG_WARNING("BLE image transfer stopped with errors. Reason: %d.", event);
            bsp_board_led_off(BLE_OTA_ACTIVITY_LED);
            break;

        case NRF_DFU_EVT_DFU_COMPLETED:
            NRF_LOG_INFO("BLE image transferred completed. Zigbee image: %s", (ota_file_sanity_check(mp_ota_file) ? "true" : "false"));

            // Check if Zigbee image was received.
            if (ota_file_sanity_check(mp_ota_file))
            {
                if (!m_external_app_valid)
                {
                    NRF_LOG_INFO("New Zigbee image dropped due to previous validation failures.");
                    bsp_board_led_off(OTA_IMAGE_PRESENT_LED);
                    break;
                }
                else
                {
                    dfu_image_valid = true;
                }

                if (m_stack_started && ZB_JOINED())
                {
                    NRF_LOG_INFO("Insert new Zigbee image to the OTA server.");
                    zb_err_code = ZB_GET_OUT_BUF_DELAYED(insert_ota_file);
                    if (zb_err_code != RET_OK)
                    {
                        NRF_LOG_WARNING("New image not inserted due to lack of Zigbee memory buffers.");
                    }
                }
                else
                {
                    NRF_LOG_INFO("Inserting new Zigbee image postponed. Waiting for the device to join Zigbee network.");
                }

                /* Disconnect from the peer. */
                ret_code_t err_code = ble_dfu_transport_disconnect();
                if (err_code != NRF_SUCCESS)
                {
                    NRF_LOG_ERROR("Unable to disconnect from the BLE DFU peer. Status: %d", err_code);
                }
            }
            else
            {
                NRF_LOG_INFO("New firmware for OTA server downloaded.");
                dfu_image_valid = true;

                if (ble_transport_active)
                {
                    NRF_LOG_INFO("Disconnect from BLE peer.");
                    UNUSED_RETURN_VALUE(app_sched_event_put(NULL, 0, ble_transport_disable));
                }
                else
                {
                    NRF_LOG_INFO("Reset device.");
                    UNUSED_RETURN_VALUE(ZB_SCHEDULE_CALLBACK(reset_ota_server, 0));
                }
            }
            break;

        default:
            NRF_LOG_INFO("Unhandled BLE DFU event: %d", event);
            break;
    }
}

nrf_dfu_result_t nrf_dfu_validation_post_external_app_execute(dfu_init_command_t const * p_init, bool is_trusted)
{
    NRF_LOG_INFO("Executing nrf_dfu_validation_post_external_app_execute\r\n");

    (void)p_init;
    (void)is_trusted;

    if (ota_file_sanity_check(mp_ota_file))
    {
        m_external_app_valid = true;
        NRF_LOG_INFO("Downloaded image is valid.");
        return NRF_DFU_RES_CODE_SUCCESS;
    }
    else
    {
        m_external_app_valid = false;
        NRF_LOG_INFO("Downloaded image is invalid.");
        return NRF_DFU_RES_CODE_INVALID;
    }
}
#endif

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

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
                UNUSED_RETURN_VALUE(bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING));
#else /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */
                m_stack_started = ZB_TRUE;
#endif /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */

                /* Look for the zigbee image file header. */
                if (ota_file_sanity_check(mp_ota_file))
                {
#ifdef SOFTDEVICE_PRESENT
                    /* Check if the whole zigbee image was received by the device
                     * before advertising it as a new, valid zigbee image.
                     *
                     * NOTE: Normally checking the bank code value is enough to
                     *       validate image, although there is one exception:
                     *       if the device did not reset between receiving an image
                     *       and joininng the zigbee network, the bank code value is still
                     *       not updated (done only by the bootloader code).
                     *       In such situation it is enough to check m_external_app_valid
                     *       flag value, as it can be set to true only by the DFU, at the post
                     *       validation phase.
                     */
                    if ((!nrf_dfu_validation_valid_external_app()) && (!m_external_app_valid))
                    {
                        NRF_LOG_INFO("DFU validation failed (incorrect bank code). Drop zigbee image.");
                        bsp_board_led_off(OTA_IMAGE_PRESENT_LED);
                        break;
                    }
                    else
#endif
                    {
                        NRF_LOG_INFO("New zigbee image found. Schedule image insertion.");
                        UNUSED_RETURN_VALUE(ZB_SCHEDULE_ALARM(insert_ota_file, param, ZIGBEE_OTA_IMAGE_NOTIFY_DELAY));
                        param = 0;
                    }
                }
                else
                {
                    NRF_LOG_INFO("No zigbee image found inside internal flash.");
                }
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

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
        case ZB_ZDO_SIGNAL_SKIP_STARTUP:
            m_stack_started = ZB_TRUE;
            break;
#endif /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */

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
    zb_ieee_addr_t ieee_addr;
    ret_code_t     ret = NRF_SUCCESS;

    UNUSED_VARIABLE(ret);
    UNUSED_VARIABLE(m_stack_started);

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) && ZIGBEE_OTA_SERVER_USE_CLI && defined(APP_USBD_ENABLED) && APP_USBD_ENABLED
    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);
    nrf_drv_clock_lfclk_request(NULL);
#endif

    /* Initialize timers, logging and LEDs */
    timers_init();
    log_init();
    bsp_board_init(BSP_INIT_LEDS);
    scheduler_init();
    power_driver_init();

#ifdef SOFTDEVICE_PRESENT
    /* Initialize DFU module. */
    UNUSED_RETURN_VALUE(nrf_dfu_settings_init(true));
    UNUSED_RETURN_VALUE(nrf_dfu_req_handler_init(dfu_observer));

    /* Initialize BLE DFU transport and start advertising. */
    ret = ble_dfu_transport_init(dfu_observer);
    APP_ERROR_CHECK(ret);
#endif

    uint32_t bootloader_addr_actual   = BOOTLOADER_ADDRESS;
    uint32_t bootloader_addr_expected = BOOTLOADER_START_ADDR;

    /* Check if bootloader start address is consistent with UICR register contents. */
    if ((bootloader_addr_expected != bootloader_addr_actual) && (bootloader_addr_actual != 0xFFFFFFFF))
    {
        NRF_LOG_ERROR("Incorrect bootloader start address. Set BOOTLOADER_START_ADDR to 0x%x", BOOTLOADER_ADDRESS);
        NRF_LOG_FINAL_FLUSH();
        ASSERT(0);
    }

    /* Sanity check for the OTA Upgrade file */
    mp_ota_file = (ota_upgrade_test_file_t *)(UPGRADE_IMAGE_OFFSET);
    if (!ota_file_sanity_check(mp_ota_file))
    {
#ifndef SOFTDEVICE_PRESENT
        // There is no way to obtain a correct Zigbee OTA firmware - halt.
        for(;;);
#endif
    }

    /* Set ZigBee stack logging level and traffic dump subsystem. */
    ZB_SET_TRACE_LEVEL(ZIGBEE_TRACE_LEVEL);
    ZB_SET_TRACE_MASK(ZIGBEE_TRACE_MASK);
    ZB_SET_TRAF_DUMP_OFF();

    /* Initialize the attributes */
    ota_server_attr_init();

    ZB_INIT("ota_server");

    /* Set device address to the value read from FICR registers. */
    zb_osif_get_ieee_eui64(ieee_addr);
    zb_set_long_address(ieee_addr);

    zb_set_network_router_role(IEEE_CHANNEL_MASK);
    zb_set_max_children(MAX_CHILDREN);
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));

    /* Register OTA Client device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&ota_upgrade_server_ctx);

    zb_zcl_ota_upgrade_init_server(OTA_ENDPOINT, next_data_ind_cb);

#if defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1)
    /* Initialize the Zigbee CLI subsystem */
    zb_cli_init(CLI_AGENT_ENDPOINT);

    /* Set the endpoint receive hook */
    ZB_AF_SET_ENDPOINT_HANDLER(CLI_AGENT_ENDPOINT, cli_agent_ep_handler);

    /* Start ZigBee stack. */
    while(1)
    {
        if (m_stack_started)
        {
            zboss_main_loop_iteration();
        }
        app_sched_execute();
        zb_cli_process();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    }
#else /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */
    zb_ret_t zb_err_code = zboss_start();
    ZB_ERROR_CHECK(zb_err_code);
    while(1)
    {
        zboss_main_loop_iteration();
        app_sched_execute();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    }
#endif /* defined(ZIGBEE_OTA_SERVER_USE_CLI) && (ZIGBEE_OTA_SERVER_USE_CLI == 1) */
}
