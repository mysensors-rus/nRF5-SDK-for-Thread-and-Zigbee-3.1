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
#include <stdlib.h>
#include <ctype.h>

#include "nrf_log.h"
#include "app_error.h"

#include "nus.h"

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                               /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static uint16_t      m_conn_handle          = BLE_CONN_HANDLE_INVALID;          /**< Handle of the current connection. */
static uint16_t      m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;     /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t    m_adv_uuids[]          =                                   /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};
static nus_entry_t   m_nus_table[NUS_CMD_NUM_MAX];                              /**< Table of NUS commands entries. */
static nus_handler_t m_default_handler;                                         /**< Default Handler of the NUS commands. */

/**@brief Function which finds a digit in the string of chars of a given length.
 * 
 * @param[in] p_str   Pointer to the char array.
 * @param[in] length  Length of this array.
 * 
 * @return            NULL if digit not found, pointer to it if found.
 */
static char * get_next_number_position(char * p_str, uint16_t length)
{
    int i;

    for (i = 0; i < length; i++)
    {
        if (isdigit(p_str[i]))
        {
            return &p_str[i];
        }
    }
    return NULL;
}

/**@brief Function for handling commands received from the Nordic UART Service.
 *
 * @details This function will check received data against the commands and
 *          issue a corresponding callback upon successfull match.
 *
 * @param[in] p_command_str  Command string received over UART.
 * @param[in] length         Length of the data.
 */
static void nus_command_handler(const uint8_t * p_command_str, uint16_t length)
{
    char       command[NUS_CMD_LENGTH_MAX + 1];
    uint16_t   len = length;
    char     * p_str = command;
    char     * p_str_end;
    int        n_of_args = 0;
    int        bytes_to_shift;
    uint32_t   args[NUS_CMD_MAX_ARGUMENTS] = {0};

    /* Check the length of the incoming string. */
    if (len > NUS_CMD_LENGTH_MAX)
    {
        /* Definitely not a command, issue default handler. */
        NRF_LOG_INFO("NUS string too long, issuing default handler.");
        ASSERT(m_default_handler);
        m_default_handler(p_command_str, length);
        return;
    }

    /* Copy the string (adding a null-terminator) so we can pass it to the
     * default handler if everything fails. */
    strncpy(command, (char *) p_command_str, len);
    command[len] = '\0';

    do 
    {
        /* Find the next digit in the string. */
        p_str = get_next_number_position(p_str, len - (p_str - command));

        if (p_str)
        {
            /* Parse and store the digit ... */
            args[n_of_args++] = strtol(p_str, &p_str_end, 10);
            /* ... and replace it with a placeholder so that we can compare it with the commands in the end. */
            *p_str = ASTERIX_DELIMITER;
            bytes_to_shift = p_str_end - p_str - 1;
            memmove(p_str + 1, p_str + 1 + bytes_to_shift, command + len - p_str_end + 1);
            len -= bytes_to_shift;
        }
    } while (p_str != NULL && n_of_args != NUS_CMD_MAX_ARGUMENTS);

    int i;
    for (i = 0; i < NUS_CMD_NUM_MAX; i++)
    {
        /* Looking for a match... (ensure that we're not matching empty-strings) */
        if ((!strcmp(m_nus_table[i].cmd, command)) && (command[0] != '\0'))
        {
            /* Pass all the arguments - the callback can handle only a subset. */
            m_nus_table[i].cb(n_of_args, args[0], args[1], args[2], args[3], args[4], args[5]);
            return;
        }
    }

    /* No command was matched, call the default handler */
    NRF_LOG_INFO("No NUS command was matched, calling default handler");
    ASSERT(m_default_handler);
    /* Call the handler with all the information prior to the parsing. */
    m_default_handler(p_command_str, length);
}

/**@brief Function for handling BLE events related to NUS.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void nus_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t                              err_code;

    UNUSED_PARAMETER(p_context);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

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


/**@brief Function for handling events from the GATT library.
 *
 * @param[in]  p_gatt  Reference to the GATT instance structure that contains status information for the GATT module.
 * @param[in]  p_evt   Reference to the GATT event structure.
 */
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
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

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and
 *          pass it to the zigbee command handler.
 *
 * @param[in] p_evt  Nordic UART Service event.
 */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        NRF_LOG_DEBUG("Received data from BLE NUS.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

        nus_command_handler(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
    }
}

/**@brief Function for handling commands received from the Nordic UART Service
 *        in case the command was not recognised - a fallback handler.
 *
 * @param[in] p_command_str  Command string received over UART.
 * @param[in] length         Length of the data.
 */
static void nus_command_default_handler(const uint8_t * p_command_str, uint16_t length)
{
    NRF_LOG_INFO("Unrecognized NUS command received:");
    NRF_LOG_HEXDUMP_INFO(p_command_str, length);
}

/**@brief Function for initializing the GATT library. */
static void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
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
                                          (const uint8_t *)NUS_DEVICE_NAME,
                                          strlen(NUS_DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
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

/**@brief Function for initializing NUS service.
 * 
 * @param[in] handler Default Handler to use. If NULL,
 *                    then nus_command_default_handler is used.
 */
static void nus_service_init(nus_handler_t handler)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init_struct;

    memset(&nus_init_struct, 0, sizeof(nus_init_struct));

    nus_init_struct.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init_struct);
    APP_ERROR_CHECK(err_code);

    if (handler)
    {
        m_default_handler = handler;
    }
    else
    {
        m_default_handler = nus_command_default_handler;
    }
}

/**@brief Function for initializing NUS commander module.
 * 
 * @param[in] handler Default Handler to use. If NULL,
 *                    then nus_command_default_handler is used.
 */
void nus_init(nus_handler_t handler)
{
    uint32_t err_code;

    NRF_SDH_BLE_OBSERVER(m_nus_observer, APP_NUS_OBSERVER_PRIO, nus_evt_handler, NULL);
    
    gap_params_init();
    gatt_init();
    nus_service_init(handler);
    advertising_init();
    conn_params_init();

    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function to set the NUS default handler. */
void nus_set_default_handler(nus_handler_t handler)
{
    m_default_handler = handler;
}

/**@brief Function to add a command. */
bool nus_add_command(char * cmd, nus_callback_t cb)
{
    if (strlen(cmd) >= NUS_CMD_LENGTH_MAX)
    {
        /* Command too long. */
        return false;
    }

    char two_delimiters[] = {ASTERIX_DELIMITER, ASTERIX_DELIMITER};
    if (strstr(cmd, two_delimiters))
    {
        /* Command is malformed. */
        return false;
    }

    int i;
    for (i = 0; i < NUS_CMD_NUM_MAX; i++)
    {
        /* Either a free row was found (then paste a new command there)
         * OR
         * a command was defined previously, then just update the callback */
        if (*(m_nus_table[i].cmd) == '\0')
        {
            strncpy(m_nus_table[i].cmd, cmd, NUS_CMD_LENGTH_MAX);
            m_nus_table[i].cb = cb;
            return true;
        }

        if (!strcmp(cmd, m_nus_table[i].cmd))
        {
            m_nus_table[i].cb = cb;
            return true;
        }
    }

    return false;
}

/**@brief Function to remove a command. */
bool nus_remove_command(char * cmd)
{
    int i;

    for (i = 0; i < NUS_CMD_NUM_MAX; i++)
    {
        if(!strcmp(cmd, m_nus_table[i].cmd))
        {
            *(m_nus_table[i].cmd) = '\0';
            m_nus_table[i].cb = NULL;
            return true;
        }
    }

    return false;
}
