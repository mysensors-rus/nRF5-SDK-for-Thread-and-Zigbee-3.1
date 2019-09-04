/**
 * Copyright (c) 2017 - 2019, Nordic Semiconductor ASA
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
 * @defgroup ble_sdk_uart_over_ble_and_thread_coap_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    Thread CoAP and UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service
 * and Thread CoAP service. This application uses the @ref srvlib_conn_params module.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_assert.h"
#include "nrf_clock.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_scheduler.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "thread_coap_utils.h"
#include "thread_utils.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/platform/radio.h>

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define DEVICE_NAME                     "Nordic_UART"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                18000                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(50, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(1000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define THREAD_TIMEOUT                  APP_TIMER_TICKS(10000)                      /**< Time for Thread mode to establish connection and send data. When expired the application switches to BLE mode. */

#define BSP_BLE_ENABLE                  BSP_INDICATE_USER_STATE_0                   /**< BSP state indicates that BLE is active protocol. */
#define BSP_THREAD_ENABLE               BSP_INDICATE_USER_STATE_1                   /**< BSP state indicates that Thread is active protocol. */

#define LED_STRING                      "led"                                       /**< BLE UART string used to request switching to Thread mode. */

#define SCHED_QUEUE_SIZE                32                                          /**< Maximum number of events in the scheduler queue. */
#define SCHED_EVENT_DATA_SIZE           APP_TIMER_SCHED_EVENT_DATA_SIZE             /**< Maximum app_scheduler event size. */

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t       m_conn_handle          = BLE_CONN_HANDLE_INVALID;             /**< Handle of the current connection. */
static uint16_t       m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;        /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t     m_adv_uuids[]          =                                      /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

APP_TIMER_DEF(m_thread_timeout);                                                    /**< Timer used to expire Thread connection in case of attachment failure. */

static volatile enum
{
    CONNECTION_STATE_IDLE,                                                          /**< The application is in the idle state. */
    CONNECTION_STATE_BLE,                                                           /**< The application is in the BLE mode. */
    CONNECTION_STATE_BLE_DISCONNECT,                                                /**< The application is scheduled to be disconnected from BLE connection. */
    CONNECTION_STATE_BLE_DISCONNECTING,                                             /**< The application is disconnecting from its BLE connection. */
    CONNECTION_STATE_BLE_DISABLE,                                                   /**< The application is disabling BLE mode. */
    CONNECTION_STATE_THREAD_ATTACH,                                                 /**< The application is attaching to a Thread network. */
    CONNECTION_STATE_THREAD_SEND,                                                   /**< The application is attached to a Thread network and requests sending a CoAP command. */
    CONNECTION_STATE_THREAD_SENT,                                                   /**< The application has requested sending a CoAP command. */
    CONNECTION_STATE_THREAD_DISABLE,                                                /**< The application is disabling Thread mode. */
} m_connection_state;                                                               /**< State of the application. */


static void switch_ble_to_thread(void);
static void switch_thread_to_ble(void);
static void thread_message_start_sending(void);


/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

        if (p_evt->params.rx_data.length == strlen(LED_STRING) &&
            strncmp((char *)p_evt->params.rx_data.p_data, LED_STRING, strlen(LED_STRING)) == 0)
        {
            thread_message_start_sending();
        }
    }
}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;

    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            if (m_connection_state == CONNECTION_STATE_BLE_DISCONNECTING)
            {
                m_connection_state = CONNECTION_STATE_BLE_DISABLE;
            }

            break;

#ifndef S140
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;
#endif

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;
#if !defined (S112)
         case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        {
            ble_gap_data_length_params_t dl_params;

            // Clearing the struct will effectivly set members to @ref BLE_GAP_DATA_LENGTH_AUTO
            memset(&dl_params, 0, sizeof(ble_gap_data_length_params_t));
            err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dl_params, NULL);
            APP_ERROR_CHECK(err_code);
        } break;
#endif //!defined (S112)
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;

            req = p_ble_evt->evt.gatts_evt.params.authorize_request;

            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, 64);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_KEY_2:
            thread_message_start_sending();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for placing the application in low power state while waiting for events.
 */
static void power_manage(void)
{
    switch (m_connection_state)
    {
        case CONNECTION_STATE_IDLE:
        case CONNECTION_STATE_BLE_DISABLE:
            break;

        case CONNECTION_STATE_BLE:
        case CONNECTION_STATE_BLE_DISCONNECT:
        case CONNECTION_STATE_BLE_DISCONNECTING:
        {
            uint32_t err_code = sd_app_evt_wait();
            APP_ERROR_CHECK(err_code);
            break;
        }

        case CONNECTION_STATE_THREAD_ATTACH:
        case CONNECTION_STATE_THREAD_SEND:
        case CONNECTION_STATE_THREAD_SENT:
        case CONNECTION_STATE_THREAD_DISABLE:
        {
            otInstance * p_instance = thread_ot_instance_get();

            if (!otTaskletsArePending(p_instance) &&
                ((otPlatRadioGetState(p_instance) == OT_RADIO_STATE_SLEEP) ||
                 (otPlatRadioGetState(p_instance) == OT_RADIO_STATE_DISABLED)))
            {
                __WFE();
            }

            break;
        }
    }
}


/**@brief Disable BLE stack.
 */
static void ble_stack_deinit(void)
{
    uint32_t err_code;

    err_code = nrf_sdh_disable_request();
    APP_ERROR_CHECK(err_code);

    // Neither SoftDevice clears clock events after disabling, nor clock driver before enabling.
    // When the clock driver is enabled there is a pending event in the clock peripheral that leads
    // to incorrect state of the driver. Following workaround prevents this problem.
    nrf_clock_event_clear(NRF_CLOCK_EVENT_HFCLKSTARTED);

    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function called when the Thread node changes it's state.
 */
static void thread_state_changed_callback(uint32_t aFlags, void *aContext)
{
    uint32_t err_code;

    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        switch (otThreadGetDeviceRole(aContext))
        {
            case OT_DEVICE_ROLE_DETACHED:
            case OT_DEVICE_ROLE_DISABLED:
                err_code = bsp_indication_set(BSP_THREAD_ENABLE);
                APP_ERROR_CHECK(err_code);
                break;

            case OT_DEVICE_ROLE_CHILD:
                err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
                APP_ERROR_CHECK(err_code);

                if (m_connection_state == CONNECTION_STATE_THREAD_ATTACH)
                {
                    m_connection_state = CONNECTION_STATE_THREAD_SEND;
                }
                break;

            default:
                break;
        }
    }
}


/**@brief Enter sending CoAP request state.
 */
static void thread_message_start_sending(void)
{
    NRF_LOG_INFO("Start swithing to Thread protocol");

    if (m_connection_state != CONNECTION_STATE_BLE)
    {
        return;
    }

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        m_connection_state = CONNECTION_STATE_BLE_DISCONNECT;
    }
    else
    {
        m_connection_state = CONNECTION_STATE_BLE_DISABLE;
    }
}


/**@brief Send CoAP request using Thread network.
 */
static void thread_message_send(uint8_t command)
{
    thread_coap_utils_multicast_light_request_send(command,
                                                   THREAD_COAP_UTILS_MULTICAST_LINK_LOCAL);
    m_connection_state = CONNECTION_STATE_THREAD_SENT;
}


/**@brief Thread connection expired.
 */
static void thread_timeout(void * p_context)
{
    (void) p_context;

    m_connection_state = CONNECTION_STATE_THREAD_DISABLE;
}


/**@brief Execute appropriate action for current application state.
 */
static void application_fsm_process(void)
{
    uint32_t     err_code;

    switch (m_connection_state)
    {
        case CONNECTION_STATE_BLE_DISCONNECT:
            NRF_LOG_INFO("Disconnecting from BLE connection");

            m_connection_state = CONNECTION_STATE_BLE_DISCONNECTING;

            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case CONNECTION_STATE_BLE_DISABLE:
            switch_ble_to_thread();

            err_code = app_timer_start(m_thread_timeout, THREAD_TIMEOUT, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case CONNECTION_STATE_THREAD_ATTACH:
            thread_process();
            break;

        case CONNECTION_STATE_THREAD_SEND:
            thread_message_send(THREAD_COAP_UTILS_LIGHT_CMD_TOGGLE);
            thread_process();
            break;

        case CONNECTION_STATE_THREAD_SENT:
        {
            otInstance * p_instance = thread_ot_instance_get();
            err_code = app_timer_stop(m_thread_timeout);
            APP_ERROR_CHECK(err_code);

            if (!otTaskletsArePending(p_instance)    &&
                !otLinkIsInTransmitState(p_instance) &&
                ((otPlatRadioGetState(p_instance) == OT_RADIO_STATE_SLEEP) ||
                 (otPlatRadioGetState(p_instance) == OT_RADIO_STATE_DISABLED)))
            {
                m_connection_state = CONNECTION_STATE_THREAD_DISABLE;
            }
            else
            {
                thread_process();
                break;
            }
        }
        // fall through

        case CONNECTION_STATE_THREAD_DISABLE:
            switch_thread_to_ble();
            break;

        default:
            break;
    }
}

/**@brief Initialize the Thread stack.
 */
static void thread_instance_init(void)
{
    uint32_t err_code;

    thread_configuration_t thread_configuration =
    {
        .radio_mode            = THREAD_RADIO_MODE_RX_OFF_WHEN_IDLE,
        .autocommissioning     = true,
        .poll_period           = 60000,
        .default_child_timeout = 240000,
    };

    thread_init(&thread_configuration);
    thread_state_changed_callback_set(thread_state_changed_callback);

    m_connection_state = CONNECTION_STATE_THREAD_ATTACH;

    err_code = bsp_indication_set(BSP_THREAD_ENABLE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Initialize application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_thread_timeout, APP_TIMER_MODE_SINGLE_SHOT, thread_timeout);
    APP_ERROR_CHECK(err_code);
}

/**@brief Initialize and enable BLE stack.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

    err_code = bsp_indication_set(BSP_BLE_ENABLE);
    APP_ERROR_CHECK(err_code);

    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();

    m_connection_state = CONNECTION_STATE_BLE;

    NRF_LOG_INFO("BLE UART Start!\r\n");
    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief Switch from BLE to Thread.
 */
static void switch_ble_to_thread(void)
{

    ASSERT(m_connection_state == CONNECTION_STATE_BLE_DISABLE);

    ble_stack_deinit();
    thread_instance_init();
}


/**@brief Switch from Thread to BLE.
 */
static void switch_thread_to_ble(void)
{
    ASSERT(m_connection_state == CONNECTION_STATE_THREAD_DISABLE);

    thread_deinit();
    ble_stack_init();
}


/**@brief Application main function.
 */
int main(void)
{
    bool erase_bonds;

    // Initialize.
    log_init();
    scheduler_init();
    timers_init();

    buttons_leds_init(&erase_bonds);
    ble_stack_init();

    // Start execution.
    NRF_LOG_INFO("BLE Thread switched MTD example started.");

    // Enter main loop.
    for (;;)
    {
        application_fsm_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            power_manage();
        }
    }
}


/**
 * @}
 */
