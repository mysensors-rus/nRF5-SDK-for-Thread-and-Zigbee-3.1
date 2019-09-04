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
#ifndef NUS_H__
#define NUS_H__

#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh.h"
#include "nrf_ble_gatt.h"

#include "ble_nus.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_hci.h"

#include "app_timer.h"
#include "bsp_btn_ble.h"

#define APP_NUS_OBSERVER_PRIO               1                                   /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG                1                                   /**< A tag identifying the SoftDevice BLE configuration. */
#define NUS_SERVICE_UUID_TYPE               BLE_UUID_TYPE_VENDOR_BEGIN          /**< UUID type for the Nordic UART Service (vendor specific). */
#define APP_ADV_DURATION                    18000                               /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define APP_ADV_INTERVAL                    320                                 /**< The advertising interval (in units of 0.625 ms. This value corresponds to 200 ms). */
#define MIN_CONN_INTERVAL                   MSEC_TO_UNITS(200, UNIT_1_25_MS)    /**< Minimum acceptable connection interval (200 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL                   MSEC_TO_UNITS(300, UNIT_1_25_MS)    /**< Maximum acceptable connection interval (300 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                       0                                   /**< Slave latency. */
#define CONN_SUP_TIMEOUT                    MSEC_TO_UNITS(4000, UNIT_10_MS)     /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY      APP_TIMER_TICKS(5000)               /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(30000)              /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT        3                                   /**< Number of attempts before giving up the connection parameter negotiation. */
#define NRF_BLE_GATT_ATT_MTU_DEFAULT        NRF_SDH_BLE_GATT_MAX_MTU_SIZE       /**< Requested ATT_MTU size. This value most not be greater than NRF_SDH_BLE_GATT_MAX_MTU_SIZE. */
#define NUS_CMD_LENGTH_MAX                  16                                  /**< Maximum length of the command. */
#define NUS_CMD_NUM_MAX                     20                                  /**< Maximum amount of commands to be handled. */
#define ASTERIX_DELIMITER                   '*'                                 /**< Symbol meaning the integer parameter placeholder. */
#define NUS_CMD_MAX_ARGUMENTS               6                                   /**< Maximum number of integer parameters in a command. */

/**@brief Macro to add NUS command. Basically it just allows the user not to cast the callback to nus_callback_t.
 */
#define NUS_ADD_COMMAND(cmd, fun)           nus_add_command(cmd, (nus_callback_t) fun)

typedef char nus_command_t[NUS_CMD_LENGTH_MAX];

/**@brief Callback which is meant to be issued when a NUS command is matched.
 * 
 * @param[in] count     Number of arguments which are passed to the callback.
 * @param     va_list   Arguments.
 * 
 */
typedef void (*nus_callback_t)(int count, ...);
/**@brief Default callback which is meant to be issued when a NUS string couldn't match any command.
 * 
 * @details The string is not necessarily null-terminated.
 * 
 * @param[in] p_command_str First byte of the string.
 * @param[in] length        Number of bytes in the string.
 */
typedef void (*nus_handler_t)(const uint8_t * p_command_str, uint16_t length);

typedef struct nus_entry_s
{
    nus_command_t  cmd;
    nus_callback_t cb;
} nus_entry_t;

/**@brief Function to initialise NUS Command service.
 * 
 * @param[in] handler Default Handler to use. If NULL,
 *                    then nus_command_default_handler is used.
 */
void nus_init(nus_handler_t handler);

/**@brief Function to add the NUS command.
 * 
 * @details If the command was already defined, its callback shall
 *          be overwritten.
 * 
 * @param[in] cmd The string name of the command to add.
 * @param[in] cb  Callback to associate with the command.
 * 
 * @return        true if adding was successful,
 *                false if there is no more space for commands.
 */
bool nus_add_command(char * cmd, nus_callback_t cb);

/**@brief Function to remove the NUS command.
 * 
 * @param[in] cmd The string name of the command to remove.
 * 
 * @return        true if removal was successful,
 *                false if the command was not found.
 */
bool nus_remove_command(char * cmd);

/**@brief   Function to set the NUS default handler.
 * 
 * @details The default handler fires up when the command
 *           could not be matched, therefore providing ways to
 *           handle non-trivial cases.
 * 
 * @param[in] handler The function to use as a handler.
 */
void nus_set_default_handler(nus_handler_t handler);


#endif /* NUS_H__ */
