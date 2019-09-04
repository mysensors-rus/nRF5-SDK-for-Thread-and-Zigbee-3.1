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
 * @defgroup ble_sdk_app_central_and_thread_sed_main main.c
 * @{
 * @ingroup ble_sdk_app_central_and_thread_sed_main
 * @brief Proximity Application and Thread CoAP Server main file.
 *
 * This application shows dynamic multiprotocol support for Thread and Bluetooth Low Energy.
 * Thread operates on 802.15.4 radio during Bluetooth Low Energy radio's inactive time.
 *
 * This file contains the source code for a sample proximity application using the
 * Immediate Alert, Link Loss and Tx Power services. The application acts as a Bluetooth
 * Low Energy central device and a Thread Sleepy End Device.
 */

#include "app_scheduler.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_gap.h"
#include "boards.h"
#include "bsp_thread.h"
#include "bsp_btn_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_log.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "thread_utils.h"

#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/platform/platform-softdevice.h>

#define CENTRAL_SCANNING_LED      BSP_LED_2_MASK                     /**< Scanning LED will be on when the device is scanning. */
#define CENTRAL_CONNECTED_LED     BSP_LED_1_MASK                     /**< Connected LED will be on when the device is connected. */

#define SCAN_INTERVAL             MSEC_TO_UNITS(120, UNIT_0_625_MS)  /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW               MSEC_TO_UNITS(30, UNIT_0_625_MS)   /**< Determines scan window in units of 0.625 millisecond. */
#define SCAN_DURATION             MSEC_TO_UNITS(5000, UNIT_10_MS)    /**< Determines scan timeout in units of 10 milliseconds. */

#define MIN_CONNECTION_INTERVAL   MSEC_TO_UNITS(100, UNIT_1_25_MS)   /**< Determines minimum connection interval in milliseconds. */
#define MAX_CONNECTION_INTERVAL   MSEC_TO_UNITS(200, UNIT_1_25_MS)    /**< Determines maximum connection interval in milliseconds. */
#define SLAVE_LATENCY             0                                  /**< Determines slave latency in terms of connection events. */
#define SUPERVISION_TIMEOUT       MSEC_TO_UNITS(4000, UNIT_10_MS)    /**< Determines supervision time-out in units of 10 milliseconds. */

#define APP_BLE_CONN_CFG_TAG      1                                  /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO     3                                  /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define SCHED_QUEUE_SIZE          32                                 /**< Maximum number of events in the scheduler queue. */
#define SCHED_EVENT_DATA_SIZE     APP_TIMER_SCHED_EVENT_DATA_SIZE    /**< Maximum app_scheduler event size. */

#define TARGET_UUID               BLE_UUID_IMMEDIATE_ALERT_SERVICE   /**< Target device name that the application is looking for. */

#define MAX_NUM_OF_KNOWN_DEVICES  16                                 /**< Maximum number of devices that can be recognized as already scanned. */

/**@brief Array of scanned devices' addresses. */
typedef struct
{
    ble_gap_addr_t addr[MAX_NUM_OF_KNOWN_DEVICES];
    uint8_t        count;
} ble_scanned_devices_t;

/**@brief Address of target device that the application is looking for. */
typedef struct
{
    ble_gap_addr_t addr;
    bool           found;
} ble_target_device_t;

static bool                  m_shall_rescan = false;                   /**< Flag that determines if scanning should be done again. */
static uint16_t              m_conn_handle  = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */
static ble_target_device_t   m_target_device;                          /**< Target device. */
static ble_scanned_devices_t m_scanned_devices;                        /**< Array of scanned devices. */

/**@brief Parameters used when scanning. */
static ble_gap_scan_params_t const m_scan_params =
{
    .active        = 1,
    .interval      = SCAN_INTERVAL,
    .window        = SCAN_WINDOW,
    .timeout       = SCAN_DURATION,
    .scan_phys     = BLE_GAP_PHY_1MBPS,
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
};

/**<@brief Buffer where advertising reports will be stored by the SoftDevice. */
static uint8_t m_scan_buffer_data[BLE_GAP_SCAN_BUFFER_MIN];

/**@brief Pointer to the buffer where advertising reports will be stored by the SoftDevice. */
static ble_data_t m_scan_buffer =
{
    m_scan_buffer_data,
    BLE_GAP_SCAN_BUFFER_MIN
};

/**@brief Connection parameters requested for connection. */
static ble_gap_conn_params_t const m_connection_param =
{
    (uint16_t)MIN_CONNECTION_INTERVAL,
    (uint16_t)MAX_CONNECTION_INTERVAL,
    (uint16_t)SLAVE_LATENCY,
    (uint16_t)SUPERVISION_TIMEOUT
};

/**@brief Function for checking if a given address has already been scanned.
 *
 * @param[in] bdaddr  Bluetooth device GAP address.
 * 
 * @retval true   If device has already been scanned.
 * @retval false  Otherwise.
 */
static bool ble_device_already_scanned(const ble_gap_addr_t * p_bdaddr)
{
    for (uint32_t i = 0; i < m_scanned_devices.count; i++)
    {
        if ((0 == memcmp(p_bdaddr, &m_scanned_devices.addr[i], sizeof(ble_gap_addr_t))))
        {
            return true;
        }
    }

    return false;
}

/**@brief Function for marking a given address as already scanned.
 *
 * @param[in] bdaddr  Bluetooth device GAP address.
 */
static void ble_device_mark_as_scanned(const ble_gap_addr_t * p_bdaddr)
{
    memcpy(&m_scanned_devices.addr[m_scanned_devices.count], p_bdaddr, sizeof(ble_gap_addr_t));
    m_scanned_devices.count++;
}

/**@brief Function for marking a given address as target device.
 *
 * @param[in] bdaddr  Bluetooth device GAP address.
 */
static void ble_device_mark_as_target(const ble_gap_addr_t * p_bdaddr)
{
    memcpy(&m_target_device.addr, p_bdaddr, sizeof(ble_gap_addr_t));
    m_target_device.found = true;
}

/**@brief Function for converting an array Bluetooth Device Address representation to a string.
 *
 * @param[out] p_addr_string    Pointer to an address string.
 * @param[in]  p_gap_addr       Pointer to a GAP address structure.
 * @param[in]  addr_string_len  Length of the string buffer.
 */
static void address_to_string_convert(char * p_addr_string, const ble_gap_addr_t * p_gap_addr, uint8_t addr_string_len)
{
    ASSERT(addr_string_len >= (sizeof(p_gap_addr->addr) + 1));

    UNUSED_VARIABLE(addr_string_len);

    int8_t index = 0;

    for (int i = sizeof(p_gap_addr->addr) - 1; i >= 0; i--)
    {
        index += sprintf((p_addr_string + index), "%x", *(p_gap_addr->addr + i));
    }
}

/**@brief Function for starting a BLE scan.
 */
static void scan_start(void)
{
    ret_code_t err_code;

    memset(&m_scanned_devices, 0, sizeof(m_scanned_devices));
    memset(&m_target_device, 0, sizeof(m_target_device));

    err_code = sd_ble_gap_scan_stop();
    if (err_code != NRF_ERROR_INVALID_STATE)
    {
        APP_ERROR_CHECK(err_code);
    }
    
    err_code = sd_ble_gap_scan_start(&m_scan_params, &m_scan_buffer);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("Scanning started.")
    err_code = bsp_indication_set(BSP_INDICATE_SCANNING);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function to handle asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num     Line number of the failing ASSERT call.
 * @param[in] p_file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

/**@brief Function for handling the advertising report BLE event.
 *
 * @param[in] p_adv_report  Advertising report from the SoftDevice.
 */
static void on_adv_report(ble_gap_evt_adv_report_t const * p_adv_report)
{
    ret_code_t err_code;
    char       peer_addr[BLE_GAP_ADDR_LEN + 1];
    ble_uuid_t target_uuid;

    memset(&target_uuid, 0, sizeof(target_uuid));
    target_uuid.uuid = TARGET_UUID;
    target_uuid.type = BLE_UUID_TYPE_BLE;

    if (!ble_device_already_scanned(&p_adv_report->peer_addr))
    {
        address_to_string_convert(peer_addr, &p_adv_report->peer_addr, sizeof(peer_addr));

        NRF_LOG_INFO("  === Advertising device found ===");
        NRF_LOG_INFO("Peer address:                %s", (uint32_t)peer_addr);

        ble_device_mark_as_scanned(&p_adv_report->peer_addr);
        
        if (ble_advdata_uuid_find(p_adv_report->data.p_data, p_adv_report->data.len, &target_uuid))
        {
            ble_device_mark_as_target(&p_adv_report->peer_addr);
            NRF_LOG_INFO("Proximity service available: true");
        }
        else
        {
            NRF_LOG_INFO("Proximity service available: false");
        }

        NRF_LOG_FLUSH();
    }

    err_code = sd_ble_gap_scan_start(NULL, &m_scan_buffer);

    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE)
    {
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t            err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
        {
            char peer_addr[BLE_GAP_ADDR_LEN + 1];

            address_to_string_convert(peer_addr, &m_target_device.addr, sizeof(peer_addr));

            NRF_LOG_INFO("Connected to %s.", (uint32_t)peer_addr);

            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);

            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        } break;

        case BLE_GAP_EVT_DISCONNECTED:
        {
            NRF_LOG_INFO("Disconnected.");

            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);

            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            if (m_shall_rescan)
            {
                m_shall_rescan = false;
                scan_start();
            }
        } break;

        case BLE_GAP_EVT_ADV_REPORT:
        {
            on_adv_report(&p_gap_evt->params.adv_report);
        } break;

        case BLE_GAP_EVT_TIMEOUT:
        {
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
            {
                NRF_LOG_INFO("Connection request timed out.");
            }
            else if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN)
            {
                if (m_target_device.found)
                {
                    err_code = sd_ble_gap_connect(&m_target_device.addr,
                                                  &m_scan_params,
                                                  &m_connection_param,
                                                  APP_BLE_CONN_CFG_TAG);
                    APP_ERROR_CHECK(err_code);
                }
            }
            else
            {
                // Intentionally empty.
            }
        } break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            // Accept parameters requested by peer.
            err_code = sd_ble_gap_conn_param_update(
                p_gap_evt->conn_handle,
                &p_gap_evt->params.conn_param_update_request.conn_params);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        } break;

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

        default:
            break;
    }
}

/**@brief Function for handling SOC events.
 *
 * @param[in]   sys_evt     SoC stack event.
 * @param[in]   p_context   Unused.
 */
static void soc_evt_handler(uint32_t sys_evt, void * p_context)
{
    UNUSED_PARAMETER(p_context);

    otSysSoftdeviceSocEvtHandler(sys_evt);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated when button is pressed.
 */
static void bsp_event_handler(bsp_event_t event)
{
    ret_code_t err_code;

    switch (event)
    {
        case BSP_EVENT_KEY_0:
        {
            if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
            {
                NRF_LOG_INFO("Preparing for a rescan.")
                err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }

                m_shall_rescan = true;
            }
            else
            {
                scan_start();
            }
        } break;

        case BSP_EVENT_DISCONNECT:
        {
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        } break;

        default:
            break;
    }
}

/***************************************************************************************************
 * @section State change handling.
 **************************************************************************************************/

static void thread_state_changed_callback(uint32_t flags, void * p_context)
{
    NRF_LOG_INFO("State changed! Flags: 0x%08x Current role: %d\r\n",
                 flags, otThreadGetDeviceRole(p_context));
}

/***************************************************************************************************
 * @section Initialization
 **************************************************************************************************/

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupts.
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

    // Register a handler for SOC events.
    NRF_SDH_SOC_OBSERVER(m_soc_observer, NRF_SDH_SOC_STACK_OBSERVER_PRIO, soc_evt_handler, NULL);
}

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(void)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the log.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing the timer.
 */
static void timer_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Thread Stack
 */
static void thread_instance_init(void)
{
    thread_configuration_t thread_configuration =
    {
        .radio_mode            = THREAD_RADIO_MODE_RX_OFF_WHEN_IDLE,
        .autocommissioning     = true,
        .poll_period           = 2500,
        .default_child_timeout = 10,
    };

    thread_init(&thread_configuration);
    thread_cli_init();
    thread_state_changed_callback_set(thread_state_changed_callback);
}

/**@brief Function for initializing the scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

int main(void)
{
    // Initialize.
    log_init();
    scheduler_init();
    timer_init();
    buttons_leds_init();

    // Bluetooth initialization.
    ble_stack_init();

    // Thread initialization.
    thread_instance_init();

    // Start execution.
    NRF_LOG_INFO("BLE Thread dynamic MTD Scanner example started.");

    // Enter main loop.
    while (true)
    {
        thread_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            thread_sleep();
        }
    }
}
