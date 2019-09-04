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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy ble_thingy_master.h
 * @{
 * @ingroup zigbee_examples
 * @brief Dynamic multiprotocol example application to demonstrate control on BLE device (peripheral role) using zigbee device.
 */

#ifndef BLE_THINGY_MASTER_H__
#define BLE_THINGY_MASTER_H__

#include "ble.h"

#include "boards.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "rgb_led.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CENTRAL_SCANNING_LED                BSP_BOARD_LED_0                     /**< Scanning LED will be on when the device is scanning. */
#define CENTRAL_CONNECTED_LED               BSP_BOARD_LED_1                     /**< Connected LED will be on when the device is connected. */

#define SCAN_INTERVAL                       0x00A0                              /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW                         0x0050                              /**< Determines scan window in units of 0.625 millisecond. */
#define SCAN_DURATION                       0                                   /**< Timout when scanning. 0x0000 disables timeout. */

#define MIN_CONNECTION_INTERVAL             MSEC_TO_UNITS(200, UNIT_1_25_MS)    /**< Minimum acceptable connection interval (200 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONNECTION_INTERVAL             MSEC_TO_UNITS(300, UNIT_1_25_MS)    /**< Maximum acceptable connection interval (300 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                       0                                   /**< Determines slave latency in terms of connection events. */
#define SUPERVISION_TIMEOUT                 MSEC_TO_UNITS(4000, UNIT_10_MS)     /**< Determines supervision time-out in units of 10 milliseconds. */

#define APP_BLE_CONN_CFG_TAG                1                                   /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO               3                                   /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define BLE_UUID_BATTERY_SERVICE            0x180F                              /**< Short UUID BLE battery service. */
#define BLE_UUID_THINGY_SERVICE             0x0300                              /**< Short UUID Thingy LED & button service. */
#define BLE_UUID_THINGY_LED_CHAR            0x0301                              /**< Short UUID Thingy LED Characteristic. */
#define BLE_UUID_THINGY_BUTTON_CHAR         0x0302                              /**< Short UUID Thingy button Characteristic. */
#define APP_BLE_CONN_HANDLER_DEFAULT        0xFFFF                              /**< Default value of connection handler used when dev is not connected. */

#define APP_BLE_THINGY_DEVICE_NAME          "Thingy"                            /**< Device name to connects to. */
#define APP_BLE_THINGY_MASTER_GATT_Q_SIZE   40                                  /**< Length of the internal GATT request queue. */
#define APP_BLE_THINGY_SCANNING_TIMEOUT     3                                   /**< Period of time after which stop scanning. */

/* Thingy device events passed to the main application. */
typedef enum
{
    THINGY_BUTTON_PRESSED,
    THINGY_BUTTON_RELEASED,
    THINGY_CONNECTED,
    THINGY_DISCONNECTED,
} thingy_evt_t;

/* Structure to contain characteristic for discovery purpose */
typedef struct
{
    uint8_t                     name[20];
    uint16_t                    handle;
} characteristic_t;

/* Structure for storing used Thingy's characteristic's */
typedef struct
{
    characteristic_t            battery_level;
    characteristic_t            led;
    characteristic_t            button;
    characteristic_t            button_cccd;
    uint16_t                    conn_handle;
    bool                        frame_lock;
} thingy_device_t;

/**@brief Callback definition for intercepting thingy device events. */
typedef void (*thingy_evt_handler_t)(thingy_device_t * p_thingy, thingy_evt_t evt);

/**@brief Function to initialise internal module structure.
 *
 * @param[IN] p_thingy_dev_table Pointer to thingy device table.
 * @param[IN] thingy_cnt         Number of thingy devices.
 * @param[IN] evt_handler        Callback function for thingy devices events.
 *
 * @returns Error code.
 */
ret_code_t ble_thingy_master_init(thingy_device_t    * p_thingy_dev_table,
                                  uint8_t              thingy_cnt,
                                  thingy_evt_handler_t evt_handler);

/**@brief Function to update LED on thingy device using given parameters.
 *
 * @param[IN]  p_thingy     Pointer to thingy device.
 * @param[IN]  led_params   Pointer to structure containing led parameters to write to Thingy LED characteristic
 */
void ble_thingy_master_update_led(thingy_device_t    * p_thingy,
                                  const led_params_t * led_params);

/**@brief Scan for Thingy devices.
 *
 * This function starts BLE scan to look for Thingy:52 devices. If found
 * a BLE connection is established.
 *
 * @param[in] timeout Duration, in seconds, of the scan.
 */
void ble_thingy_master_scan(uint8_t timeout);


#ifdef __cplusplus
}
#endif

#endif // BLE_THINGY_MASTER_H__

/**
 * @}
 */
