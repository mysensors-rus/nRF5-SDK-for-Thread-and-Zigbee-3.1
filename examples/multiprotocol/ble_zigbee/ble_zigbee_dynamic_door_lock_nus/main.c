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
 * @defgroup zigbee_examples_multiprotocol_nus_door_lock main.c
 * @{
 * @ingroup  zigbee_examples
 * @brief    UART over BLE application with Zigbee HA Door Lock profile.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service
 * and a door lock operating a Zigbee network.
 * This application uses the @ref srvlib_conn_params module.
 */
#include "zboss_api.h"
#include "zboss_api_addons.h"
#include "zb_mem_config_min.h"
#include "zb_error_handler.h"
#include "zb_zcl_identify.h"
#include "zigbee_helpers.h"

#include "nus.h"

#include "app_timer.h"
#include "app_pwm.h"
#include "boards.h"
#include "bsp_btn_ble.h"
#include "fds.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define APP_BLE_OBSERVER_PRIO               1                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define IEEE_CHANNEL_MASK                   (1l << ZIGBEE_CHANNEL)                  /**< Scan only one, predefined channel to find the coordinator. */
#define DOOR_LOCK_ENDPOINT                  8                                       /**< First source endpoint used to control Door Lock. */
#define ERASE_PERSISTENT_CONFIG             ZB_FALSE                                /**< Do not erase NVRAM to save the network parameters after device reboot or power-off. NOTE: If this option is set to ZB_TRUE then do full device erase for all network devices before running other samples. */
#define ZIGBEE_NETWORK_STATE_LED            BSP_BOARD_LED_2                         /**< LED indicating that Door Lock successfully joind Zigbee network. */
#define DOOR_LOCK_STATE_LED                 BSP_BOARD_LED_3                         /**< LED indicating that the state of the door (closed - ON, open - OFF). */
#define DOOR_LOCK_CONFIG_FILE               0xBEEF                                  /**< ID of the file with the Door Lock configuration. */
#define DOOR_LOCK_CONFIG_STATE_KEY          0x1337                                  /**< ID of the key with Door Lock state inside the configuration. */

#define DOOR_LOCK_BASIC_APP_VERSION         01                                      /**< Version of the application software (1 byte). */
#define DOOR_LOCK_BASIC_STACK_VERSION       10                                      /**< Version of the implementation of the ZigBee stack (1 byte). */
#define DOOR_LOCK_BASIC_HW_VERSION          11                                      /**< Version of the hardware of the device (1 byte). */
#define DOOR_LOCK_BASIC_MANUF_NAME          "Nordic"                                /**< Manufacturer name (32 bytes). */
#define DOOR_LOCK_BASIC_MODEL_ID            "Door_Lock_v0.1"                        /**< Model number assigned by manufacturer (32-bytes long string). */
#define DOOR_LOCK_BASIC_DATE_CODE           "20180113"                              /**< First 8 bytes specify the date of manufacturer of the device in ISO 8601 format (YYYYMMDD). Th rest (8 bytes) are manufacturer specific. */
#define DOOR_LOCK_BASIC_POWER_SOURCE        ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE     /**< Type of power sources available for the device. For possible values see section 3.2.2.2.8 of ZCL specification. */
#define DOOR_LOCK_BASIC_PH_ENV              ZB_ZCL_BASIC_ENV_UNSPECIFIED            /**< Describes the type of physical environment. For possible values see section 3.2.2.2.10 of ZCL specification. */
#define DOOR_LOCK_BASIC_LOCATION_DESC       "Cave"                                  /**< Describes the physical location of the device (16 bytes). May be modified during commisioning process. */

#define NUS_COMMAND_OPEN                    "sesame"                                /**< NUS command that will unlock the door. */
#define NUS_COMMAND_CLOSE                   "hodor"                                 /**< NUS command that will lock the door. */

#define LOCK_MECHANISM_PIN                  NRF_GPIO_PIN_MAP(0,29)                  /**< Pin 0.29 - the one which drives the PWM. */
#define LOCK_MECHANISM_PWM_NAME             PWM1                                    /**< PWM instance used to drive the door lock. */
#define LOCK_MECHANISM_PWM_TIMER            2                                       /**< Timer number used by PWM. */
#define LOCK_MECHANISM_STATE_OPEN_DC        70                                      /**< Duty Cycle to maintain at the Lock Open state. */
#define LOCK_MECHANISM_STATE_CLOSE_DC       55                                      /**< Duty Cycle to maintain at the Lock Close state. */

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile Door Lock (End Device) source code.
#endif

APP_PWM_INSTANCE(LOCK_MECHANISM_PWM_NAME, LOCK_MECHANISM_PWM_TIMER);                /**< PWM Instance to steer the lock. */
static volatile bool m_fds_initialized;                                             /**< Flag which signals the FDS subsystem being initialised. */
static volatile bool m_garbage_flag;                                                /**< Flag which signals the need for garbage collector to clean up the flash. */

/* Protocol-agnostic configuration of Door Lock. */
typedef struct
{
    uint32_t is_locked;
} door_lock_configuration_t;

/* Main application customizable context. Stores all settings and static values. */
typedef struct door_lock_ctx_s
{
    door_lock_configuration_t      configuration;  // Configuration has to be word-aligned, because of FDS, hence it's first in the structure.
    zb_zcl_basic_attrs_ext_t       basic_attr;
    zb_zcl_identify_attrs_t        identify_attr;
    zb_zcl_scenes_attrs_t          scenes_attr;
    zb_zcl_groups_attrs_t          groups_attr;
    zb_zcl_door_lock_attrs_t       door_lock_attr;
} door_lock_ctx_t;

static door_lock_ctx_t m_dev_ctx;

/* Configuration record for the FDS subsystem. */
static fds_record_t const m_configuration_record =
{
    .file_id           = DOOR_LOCK_CONFIG_FILE,
    .key               = DOOR_LOCK_CONFIG_STATE_KEY,
    .data.p_data       = &m_dev_ctx.configuration,
    /* The length of a record is always expressed in 4-byte units (words). */
    .data.length_words = (sizeof(m_dev_ctx.configuration) + 3) / sizeof(uint32_t),
};

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list,
                                     &m_dev_ctx.basic_attr.zcl_version,
                                     &m_dev_ctx.basic_attr.app_version,
                                     &m_dev_ctx.basic_attr.stack_version,
                                     &m_dev_ctx.basic_attr.hw_version,
                                     m_dev_ctx.basic_attr.mf_name,
                                     m_dev_ctx.basic_attr.model_id,
                                     m_dev_ctx.basic_attr.date_code,
                                     &m_dev_ctx.basic_attr.power_source,
                                     m_dev_ctx.basic_attr.location_id,
                                     &m_dev_ctx.basic_attr.ph_env,
                                     m_dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &m_dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &m_dev_ctx.groups_attr.name_support);

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list,
                                  &m_dev_ctx.scenes_attr.scene_count,
                                  &m_dev_ctx.scenes_attr.current_scene,
                                  &m_dev_ctx.scenes_attr.current_group,
                                  &m_dev_ctx.scenes_attr.scene_valid,
                                  &m_dev_ctx.scenes_attr.name_support);

ZB_ZCL_DECLARE_DOOR_LOCK_ATTRIB_LIST(door_lock_attr_list,
                                     &m_dev_ctx.door_lock_attr.lock_state,
                                     &m_dev_ctx.door_lock_attr.lock_type,
                                     &m_dev_ctx.door_lock_attr.actuator_enabled);

ZB_HA_DECLARE_DOOR_LOCK_CLUSTER_LIST(door_lock_clusters, 
                                     door_lock_attr_list, 
                                     basic_attr_list, 
                                     identify_attr_list, 
                                     groups_attr_list, 
                                     scenes_attr_list);

ZB_HA_DECLARE_DOOR_LOCK_EP(door_lock_ep, DOOR_LOCK_ENDPOINT, door_lock_clusters);

ZB_HA_DECLARE_DOOR_LOCK_CTX(door_lock_ctx, door_lock_ep);


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    UNUSED_PARAMETER(p_context);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            bsp_board_led_off(BSP_BOARD_LED_0);
            bsp_board_led_off(BSP_BOARD_LED_1);
            break;

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

        default:
            // No implementation needed.
            break;
    }
}


/***************************************************************************************************
 * @section Initialization
 **************************************************************************************************/

/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
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
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing the application timer.
 */
static void timer_init(void)
{
    uint32_t error_code;
    error_code          = app_timer_init();
    APP_ERROR_CHECK(error_code);
}


/**@brief Functiom which essentially sets the lock state via PWM and stores the value in flash.
 * 
 * @param[in] value ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED to lock,
 *                  ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED to unlock.
 */
static zb_void_t set_lock_state(zb_uint8_t value)
{


    if (value == ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED)
    {
        m_dev_ctx.configuration.is_locked = (uint32_t) true;
        bsp_board_led_on(DOOR_LOCK_STATE_LED);
        while (app_pwm_channel_duty_set(&LOCK_MECHANISM_PWM_NAME, 0, LOCK_MECHANISM_STATE_CLOSE_DC) == NRF_ERROR_BUSY)
        {
        }
        UNUSED_RETURN_VALUE(zb_zcl_set_attr_val(DOOR_LOCK_ENDPOINT,
                                                ZB_ZCL_CLUSTER_ID_DOOR_LOCK,
                                                ZB_ZCL_CLUSTER_SERVER_ROLE,
                                                ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID,
                                                &value,
                                                ZB_FALSE));
    }
    else if (value == ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED)
    {
        m_dev_ctx.configuration.is_locked = (uint32_t) false;
        bsp_board_led_off(DOOR_LOCK_STATE_LED);
        while (app_pwm_channel_duty_set(&LOCK_MECHANISM_PWM_NAME, 0, LOCK_MECHANISM_STATE_OPEN_DC) == NRF_ERROR_BUSY)
        {
        }
        UNUSED_RETURN_VALUE(zb_zcl_set_attr_val(DOOR_LOCK_ENDPOINT,
                                                ZB_ZCL_CLUSTER_ID_DOOR_LOCK,
                                                ZB_ZCL_CLUSTER_SERVER_ROLE,
                                                ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID,
                                                &value,
                                                ZB_FALSE));
    }
    else 
    {
        NRF_LOG_WARNING("Wrong value of lock state - omitting");
        return;
    }

    ret_code_t        err_code;
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    err_code = fds_record_find(DOOR_LOCK_CONFIG_FILE, DOOR_LOCK_CONFIG_STATE_KEY, &desc, &tok);
    APP_ERROR_CHECK(err_code);
    
    err_code = fds_record_update(&desc, &m_configuration_record);
    APP_ERROR_CHECK(err_code);
    /* Set the garbage flag to dispatch the garbage collector. */
    m_garbage_flag = true;
}

/**@brief Function for initializing all clusters attributes.
 */
static void door_lock_clusters_attr_init(void)
{
    /* Basic cluster attributes data */
    m_dev_ctx.basic_attr.zcl_version   = ZB_ZCL_VERSION;
    m_dev_ctx.basic_attr.app_version   = DOOR_LOCK_BASIC_APP_VERSION;
    m_dev_ctx.basic_attr.stack_version = DOOR_LOCK_BASIC_STACK_VERSION;
    m_dev_ctx.basic_attr.hw_version    = DOOR_LOCK_BASIC_HW_VERSION;

    /* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte should
     * contain string length without trailing zero.
     *
     * For example "test" string wil be encoded as:
     *   [(0x4), 't', 'e', 's', 't']
     */
    ZB_ZCL_SET_STRING_VAL(m_dev_ctx.basic_attr.mf_name,
                          DOOR_LOCK_BASIC_MANUF_NAME,
                          ZB_ZCL_STRING_CONST_SIZE(DOOR_LOCK_BASIC_MANUF_NAME));

    ZB_ZCL_SET_STRING_VAL(m_dev_ctx.basic_attr.model_id,
                          DOOR_LOCK_BASIC_MODEL_ID,
                          ZB_ZCL_STRING_CONST_SIZE(DOOR_LOCK_BASIC_MODEL_ID));

    ZB_ZCL_SET_STRING_VAL(m_dev_ctx.basic_attr.date_code,
                          DOOR_LOCK_BASIC_DATE_CODE,
                          ZB_ZCL_STRING_CONST_SIZE(DOOR_LOCK_BASIC_DATE_CODE));

    m_dev_ctx.basic_attr.power_source = DOOR_LOCK_BASIC_POWER_SOURCE;

    ZB_ZCL_SET_STRING_VAL(m_dev_ctx.basic_attr.location_id,
                          DOOR_LOCK_BASIC_LOCATION_DESC,
                          ZB_ZCL_STRING_CONST_SIZE(DOOR_LOCK_BASIC_LOCATION_DESC));


    m_dev_ctx.basic_attr.ph_env = DOOR_LOCK_BASIC_PH_ENV;

    /* Identify cluster attributes data */
    m_dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

    /* Door Lock cluster attributes data */
    m_dev_ctx.door_lock_attr.lock_type        = ZB_ZCL_ATTR_DOOR_LOCK_LOCK_TYPE_OTHER;
    m_dev_ctx.door_lock_attr.lock_state       = m_dev_ctx.configuration.is_locked ? ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED : ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED;
    m_dev_ctx.door_lock_attr.actuator_enabled = ZB_TRUE;

    if ((bool)m_dev_ctx.configuration.is_locked == false)
    {
        set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
    }
    else
    {
        set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
    }

}


/**@brief Perform local operation - leave network.
 *
 * @param[in]   param   Reference to ZigBee stack buffer that will be used to construct leave request.
 */
static void door_lock_leave_nwk(zb_uint8_t param)
{
    zb_ret_t zb_err_code;

    // We are going to leave
    if (param)
    {
        zb_buf_t                  * p_buf = ZB_BUF_FROM_REF(param);
        zb_zdo_mgmt_leave_param_t * p_req_param;

        p_req_param = ZB_GET_BUF_PARAM(p_buf, zb_zdo_mgmt_leave_param_t);
        UNUSED_RETURN_VALUE(ZB_BZERO(p_req_param, sizeof(zb_zdo_mgmt_leave_param_t)));

        // Set dst_addr == local address for local leave
        p_req_param->dst_addr = ZB_PIBCACHE_NETWORK_ADDRESS();
        p_req_param->rejoin   = ZB_FALSE;
        UNUSED_RETURN_VALUE(zdo_mgmt_leave_req(param, NULL));
    }
    else
    {
        zb_err_code = ZB_GET_OUT_BUF_DELAYED(door_lock_leave_nwk);
        ZB_ERROR_CHECK(zb_err_code);
    }
}

/**@brief Function for starting join/rejoin procedure.
 *
 * param[in]   leave_type   Type of leave request (with or without rejoin).
 */
static zb_void_t door_lock_retry_join(zb_uint8_t leave_type)
{
    zb_bool_t comm_status;

    if (leave_type == ZB_NWK_LEAVE_TYPE_RESET)
    {
        comm_status = bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        ZB_COMM_STATUS_CHECK(comm_status);
    }
}

/**@brief Function for leaving current network and starting join procedure afterwards.
 *
 * @param[in]   param   Optional reference to ZigBee stack buffer to be reused by leave and join procedure.
 */
static zb_void_t door_lock_leave_and_join(zb_uint8_t param)
{
    if (ZB_JOINED())
    {
        // Leave network. Joining procedure will be initiated inisde ZigBee stack signal handler.
        door_lock_leave_nwk(param);
    }
    else
    {
        // Already left network. Start joining procedure.
        door_lock_retry_join(ZB_NWK_LEAVE_TYPE_RESET);

        if (param)
        {
            ZB_FREE_BUF_BY_REF(param);
        }
    }
}

/**@brief Handler of the FDS events.
 * 
 * @param[in] p_evt Pointer to the FDS context.
 */
static void fds_evt_handler(fds_evt_t const * p_evt)
{
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == FDS_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        default:
            break;
    }
}

/**@brief Function to initialise FDS storage and read the initial lock state out of it.
 */
static void storage_init(void)
{
    ret_code_t err_code;

    /* Register the handler. */
    UNUSED_RETURN_VALUE(fds_register(fds_evt_handler));

    err_code = fds_init();
    APP_ERROR_CHECK(err_code);

    /* Wait until we're initialised. */
    while (!m_fds_initialized)
    {
    }

    /* Get the record data out. */
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    err_code = fds_record_find(DOOR_LOCK_CONFIG_FILE, DOOR_LOCK_CONFIG_STATE_KEY, &desc, &tok);
    if (err_code == FDS_SUCCESS)
    {
        NRF_LOG_INFO("Previous configuration found");

        /* A config file is in flash. Let's update the RAM value. */
        fds_flash_record_t config = {0};

        /* Open the record and read its contents. */
        err_code = fds_record_open(&desc, &config);
        APP_ERROR_CHECK(err_code);

        /* Copy the configuration from flash into the configuration in the RAM. */
        memcpy(&m_dev_ctx.configuration, config.p_data, sizeof(door_lock_configuration_t));

        NRF_LOG_INFO("Loaded configuration: door is %s", m_dev_ctx.configuration.is_locked ? "locked" : "unlocked");

        /* Close the record when done reading. */
        err_code = fds_record_close(&desc);
        APP_ERROR_CHECK(err_code);
    }    
    else
    {
        NRF_LOG_INFO("Previous configuration not found, creating one");

        err_code = fds_record_write(&desc, &m_configuration_record);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for initializing LEDs and PWM.
 */
static void leds_pwm_init(void)
{
    ret_code_t       error_code;
    app_pwm_config_t pwm_cfg = APP_PWM_DEFAULT_CONFIG_1CH(5000L, LOCK_MECHANISM_PIN);

    /* Initialize LEDs - use BSP to control them. */
    error_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(error_code);
    bsp_board_leds_off();

    /* Initialize PWM running on timer 1 in order to control Door Lock servo. */
    error_code = app_pwm_init(&LOCK_MECHANISM_PWM_NAME, &pwm_cfg, NULL);
    APP_ERROR_CHECK(error_code);

    app_pwm_enable(&LOCK_MECHANISM_PWM_NAME);

}

/**@brief Callback function for handling ZCL commands.
 *
 * @param[in]   param   Reference to ZigBee stack buffer used to pass received data.
 */
static zb_void_t zcl_device_cb(zb_uint8_t param)
{
    zb_buf_t                       * p_buffer          = ZB_BUF_FROM_REF(param);
    zb_zcl_device_callback_param_t * p_device_cb_param = ZB_GET_BUF_PARAM(p_buffer, zb_zcl_device_callback_param_t);

    /* Set default response value. */
    p_device_cb_param->status = RET_OK;

    switch (p_device_cb_param->device_cb_id)
    {
        case ZB_ZCL_DOOR_LOCK_UNLOCK_DOOR_CB_ID:
            set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
            break;

        case ZB_ZCL_DOOR_LOCK_LOCK_DOOR_CB_ID:
            set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
            break;

        default:
            p_device_cb_param->status = RET_ERROR;
            break;
    }
}

/**@brief Function for initializing the Zigbee Stack.
 */
static void zigbee_init(void)
{
    zb_ieee_addr_t ieee_addr;

    /* Set ZigBee stack logging level and traffic dump subsystem. */
    ZB_SET_TRACE_LEVEL(ZIGBEE_TRACE_LEVEL);
    ZB_SET_TRACE_MASK(ZIGBEE_TRACE_MASK);
    ZB_SET_TRAF_DUMP_OFF();

    /* Initialize ZigBee stack. */
    ZB_INIT("door_lock_nus");

    /* Set device address to the value read from FICR registers. */
    zb_osif_get_ieee_eui64(ieee_addr);
    zb_set_long_address(ieee_addr);

    /* Set up Zigbee protocol main parameters. */
    zb_set_network_ed_role(IEEE_CHANNEL_MASK);
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);

    zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(300));
    zb_set_rx_on_when_idle(ZB_FALSE);

    /* Register callback for handling ZCL commands. */
    ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

    /* Register Door Lock device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&door_lock_ctx);

    /* Initialize clusters' attributes. */
    door_lock_clusters_attr_init();
}

/**@brief Lock the Lock via the BLE connection.
 * 
 * @param[in] count Unused parameter, inherited from NUS command template.
 */
static void cmd_lock(int count)
{
    UNUSED_PARAMETER(count);
    set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
}

/**@brief Unlock the Lock via the BLE connection.
 * 
 * @param[in] count Unused parameter, inherited from NUS command template.
 */
static void cmd_unlock(int count)
{
    UNUSED_PARAMETER(count);
    set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
}

/**@brief Function which adds the commands to the NUS commander.
 */
static void add_nus_commands(void)
{
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(NUS_COMMAND_OPEN, cmd_unlock));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(NUS_COMMAND_CLOSE, cmd_lock));
}

/**@brief ZigBee stack event handler.
 *
 * @param[in]   param   Reference to ZigBee stack buffer used to pass arguments (signal).
 */
void zboss_signal_handler(zb_uint8_t param)
{
    zb_zdo_app_signal_hdr_t      * p_sg_p         = NULL;
    zb_zdo_signal_leave_params_t * p_leave_params = NULL;
    zb_zdo_app_signal_type_t       sig            = zb_get_app_signal(param, &p_sg_p);
    zb_ret_t                       status         = ZB_GET_APP_SIGNAL_STATUS(param);
    zb_ret_t                       zb_err_code;

    switch(sig)
    {
        case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (status == RET_OK)
            {
                NRF_LOG_INFO("Joined network successfully");
                bsp_board_led_on(ZIGBEE_NETWORK_STATE_LED);
            }
            else
            {
                NRF_LOG_ERROR("Failed to join network. Status: %d", status);
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);
                zb_err_code = ZB_SCHEDULE_ALARM(door_lock_leave_and_join, 0, ZB_TIME_ONE_SECOND);
                ZB_ERROR_CHECK(zb_err_code);
            }
            break;

        case ZB_ZDO_SIGNAL_LEAVE:
            if (status == RET_OK)
            {
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);
                p_leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_leave_params_t);
                NRF_LOG_INFO("Network left. Leave type: %d", p_leave_params->leave_type);
                door_lock_retry_join(p_leave_params->leave_type);
            }
            else
            {
                NRF_LOG_ERROR("Unable to leave network. Status: %d", status);
            }
            break;

        case ZB_COMMON_SIGNAL_CAN_SLEEP:
            zb_sleep_now();
            break;

        case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
            if (status != RET_OK)
            {
                NRF_LOG_WARNING("Production config is not present or invalid");
            }
            break;

        default:
            /* Unhandled signal. For more information see: zb_zdo_app_signal_type_e and zb_ret_e */
            NRF_LOG_INFO("Unhandled signal %d. Status: %d", sig, status);
    }

    if (param)
    {
        ZB_FREE_BUF_BY_REF(param);
    }
}


/**@brief Function for application main entry.
 */
int main(void)
{
    zb_ret_t   zb_err_code;

    /* Initialize loging system and timers. */
    log_init();
    timer_init();

    /* Intitialise the FDS storage subsystem. */
    storage_init();

    /* Initialise the LEDs and PWM to steer the lock. */
    leds_pwm_init();

    /* Bluetooth initialization. */
    ble_stack_init();
    /* NUS Commander initialization. */
    nus_init(NULL);
    
    /* Add commands to NUS */
    add_nus_commands();

    /* Initialize Zigbee stack. */
    zigbee_init();

    /* Start execution. */
    NRF_LOG_INFO("BLE Zigbee dynamic door lock example started.");

    /** Start Zigbee Stack. */
    zb_err_code = zboss_start();
    ZB_ERROR_CHECK(zb_err_code);

    while(1)
    {
        zboss_main_loop_iteration();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        if (m_garbage_flag)
        {
            m_garbage_flag = false;
            UNUSED_RETURN_VALUE(fds_gc());
        }
    }
}


/**
 * @}
 */
