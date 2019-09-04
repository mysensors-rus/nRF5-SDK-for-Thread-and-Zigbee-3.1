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
 * @defgroup zigbee_examples_multiprotocol_nus_switch main.c
 * @{
 * @ingroup  zigbee_examples
 * @brief    UART over BLE application with Zigbee HA Color Dimmer Switch profile.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service
 * and a color dimmer light switch operating a Zigbee network.
 * This application uses the @ref srvlib_conn_params module.
 */
#include "zboss_api.h"
#include "zboss_api_addons.h"
#include "zb_mem_config_max.h"
#include "zb_error_handler.h"
#include "zigbee_color_dimmer_switch.h"
#include "zb_zcl_identify.h"
#include "zigbee_helpers.h"

#include "nus.h"

#include "app_timer.h"
#include "boards.h"
#include "bsp_btn_ble.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#ifdef ARDUINO_JOYSTICK_SHIELD_V1A
#include "joystick.h"
#endif

#define APP_BLE_OBSERVER_PRIO               1                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define IEEE_CHANNEL_MASK                   (1l << ZIGBEE_CHANNEL)                  /**< Scan only one, predefined channel to find the coordinator. */
#define LIGHT_SWITCH_ENDPOINT_I             1                                       /**< First source endpoint used to control light bulb(s). */
#define LIGHT_SWITCH_ENDPOINT_II            2                                       /**< Second source endpoint used to control light bulb(s). */
#define LIGHT_SWITCH_ENDPOINT_III           3                                       /**< Third source endpoint used to control light bulb(s). */
#define MATCH_DESC_REQ_START_DELAY          (2 * ZB_TIME_ONE_SECOND)                /**< Delay between the light switch startup and light bulb finding procedure. */
#define MATCH_DESC_REQ_TIMEOUT              (5 * ZB_TIME_ONE_SECOND)                /**< Timeout for finding procedure. */
#define ERASE_PERSISTENT_CONFIG             ZB_FALSE                                /**< Do not erase NVRAM to save the network parameters after device reboot or power-off. NOTE: If this option is set to ZB_TRUE then do full device erase for all network devices before running other samples. */
#define FINDING_N_BINDING_STATE_LED         BSP_BOARD_LED_1                         /**< LED indicating that Finding & Binding process is ongoing. */
#define ZIGBEE_NETWORK_STATE_LED            BSP_BOARD_LED_2                         /**< LED indicating that light switch successfully joind ZigBee network. */
#define ENDPOINT_SELECTED_LED               BSP_BOARD_LED_3                         /**< LED indicating which endpoint is selected (number of pulses equals the number of endpoint) */
#define LIGHT_SWITCH_BUTTON_ON              BSP_BOARD_BUTTON_0                      /**< Button ID used to switch on the light bulb. */
#define LIGHT_SWITCH_BUTTON_OFF             BSP_BOARD_BUTTON_1                      /**< Button ID used to switch off the light bulb. */
#define SLEEPY_ON_BUTTON                    BSP_BOARD_BUTTON_2                      /**< Button ID used to determine if we need the sleepy device behaviour at start (pressed means yes). */
#define SWITCH_ENDPOINT_BUTTON              BSP_BOARD_BUTTON_2                      /**< Button ID used to switch to the next Ligt Switch endpoint. */
#define FINDING_N_BINDING_BUTTON            BSP_BOARD_BUTTON_3                      /**< Button ID used to initiate Finding & Binding procedure. */

#define LIGHT_SWITCH_DIM_STEP               15                                      /**< Dim step size - increases/decreses current level (range 0x00 - 0xFE). */
#define LIGHT_SWITCH_DIM_TRANSITION_TIME    2                                       /**< Transition time for a single step operation in 0.1 sec units. 0xFFFF - immediate change. */
#define LIGHT_SWITCH_COLOR_TRANSITION_TIME  0                                       /**< Transition time for a move to hue and saturation operation in 0.1 sec units. 0xFFFF - immediate change. */

#define LIGHT_SWITCH_BUTTON_THRESHOLD       ZB_TIME_ONE_SECOND                      /**< Number of beacon intervals the button should be pressed to dimm the light bulb. */
#define LIGHT_SWITCH_BUTTON_SHORT_POLL_TMO  ZB_MILLISECONDS_TO_BEACON_INTERVAL(50)  /**< Delay between button state checks used in order to detect button long press. */
#define LIGHT_SWITCH_BUTTON_LONG_POLL_TMO   ZB_MILLISECONDS_TO_BEACON_INTERVAL(300) /**< Time after which the button state is checked again to detect button hold - the dimm command is sent again. */

#define LED_ENDPOINT_INDICATION_ON_TIME_MS  750                                     /**< Period of LED indication staying on. */
#define LED_ENDPOINT_INDICATION_OFF_TIME_MS 3000                                    /**< Period of LED indication staying off. */

/* NOTE: Any numeric value within range 0 - 999 received over BLE UART will start a delayed toggle operation. */
#define COMMAND_ON                          "n"                                     /**< UART command that will turn on found light bulb(s). */
#define COMMAND_OFF                         "f"                                     /**< UART command that will turn off found light bulb(s). */
#define COMMAND_TOGGLE                      "t"                                     /**< UART command that will turn toggle found light bulb(s). */
#define COMMAND_INCREASE                    "i"                                     /**< UART command that will increase brightness of found light bulb(s). */
#define COMMAND_DECREASE                    "d"                                     /**< UART command that will decrease brightness of found light bulb(s). */
#define COMMAND_SWITCH_NEXT_EP              "sn"                                    /**< UART command that will switch the active endpoint to the next one. */
#define COMMAND_SWITCH_PREV_EP              "sp"                                    /**< UART command that will switch the active endpoint to the previous one. */
#define COMMAND_FIND_N_BIND                 "&"                                     /**< UART command that will start the Finding & Binding procedure. */
#define COMMAND_HUE_SATURATION              "h*s*"                                  /**< UART command description that will change hue to first * and saturation to the second * of found light bulb(s). */
#define COMMAND_TOGGLE_DELAY                "*"                                     /**< UART command description that will turn toggle found light bulb(s) after * seconds. */
#define DELAYED_COMMAND_RETRY_MS            100                                     /**< If sending toggle command was impossible due tothe lack of Zigbee buffers, retry sending it after DELAYED_COMMAND_RETRY_MS ms. */

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile light switch (End Device) source code.
#endif


/* Variables used to recognize the type of button press. */
typedef struct light_switch_button_s
{
    zb_bool_t in_progress;
    zb_time_t timestamp;
} light_switch_button_t;

typedef struct find_n_bind_s
{
    zb_uint8_t current_ep;
} find_n_bind_t;

/* Main application customizable context. Stores all settings and static values. */
typedef struct light_switch_ctx_s
{
    light_switch_button_t      button;
    find_n_bind_t              find_n_bind;
} light_switch_ctx_t;


static void light_switch_send_delayed_toggle(void * p_context);
static void led_indicate_endpoint(void * p_context);
static void led_indicate_endpoint_cb(void * p_context);

APP_TIMER_DEF(m_toggle_timer);                                                      /**< APP timer that is responsible for sending a delayed Zigbee toggle command. */
APP_TIMER_DEF(m_led_indication_timer);                                              /**< APP timer that is responsible for inidicating the selected endpoint. */
APP_TIMER_DEF(m_led_indication_timer_aux);                                          /**< Auxiliary APP timer for the LED inidication timer. */


static light_switch_ctx_t m_device_ctx;

/* Declare context variable, cluster, and attribute list for first endpoint */
static zb_color_dimmer_switch_dev_ctx_t zb_dev_ctx_first;
ZB_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_ATTR_LIST(zb_dev_ctx_first, color_dimmer_switch_clusters_first);

/* Declare context variable, cluster, and attribute list for second endpoint */
static zb_color_dimmer_switch_dev_ctx_t zb_dev_ctx_second;
ZB_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_ATTR_LIST(zb_dev_ctx_second, color_dimmer_switch_clusters_second);

/* Declare context variable, cluster, and attribute list for third endpoint */
static zb_color_dimmer_switch_dev_ctx_t zb_dev_ctx_third;
ZB_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_ATTR_LIST(zb_dev_ctx_third, color_dimmer_switch_clusters_third);

/* Declare endpoints for Dimmer Switch device. */
ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_EP(color_dimmer_switch_ep_i,
                                      LIGHT_SWITCH_ENDPOINT_I,
                                      color_dimmer_switch_clusters_first);

ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_EP(color_dimmer_switch_ep_ii,
                                      LIGHT_SWITCH_ENDPOINT_II,
                                      color_dimmer_switch_clusters_second);

ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_EP(color_dimmer_switch_ep_iii,
                                      LIGHT_SWITCH_ENDPOINT_III,
                                      color_dimmer_switch_clusters_third);


/* Declare application's device context (list of registered endpoints) for Dimmer Switch device. */
ZBOSS_DECLARE_DEVICE_CTX_EP_VA(color_dimmer_switch_ctx, &color_dimmer_switch_ep_i, &color_dimmer_switch_ep_ii, &color_dimmer_switch_ep_iii);


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t                              err_code;

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

    error_code = app_timer_create(&m_toggle_timer, APP_TIMER_MODE_SINGLE_SHOT, light_switch_send_delayed_toggle);
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_create(&m_led_indication_timer, APP_TIMER_MODE_REPEATED, led_indicate_endpoint);
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_create(&m_led_indication_timer_aux, APP_TIMER_MODE_SINGLE_SHOT, led_indicate_endpoint_cb);
    APP_ERROR_CHECK(error_code);
}


/***************************************************************************************************
 * @section Zigbee stack related functions.
 **************************************************************************************************/

/**@brief Function for initializing clusters attributes.
 *
 * @param[IN]  p_device_ctx  Pointer to structure with device_ctx.
 */
static void light_switch_clusters_attr_init(zb_color_dimmer_switch_dev_ctx_t * p_device_ctx)
{
    /* Basic cluster attributes data */
    p_device_ctx->basic_attr.zcl_version   = ZB_ZCL_VERSION;
    p_device_ctx->basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;

    /* Identify cluster attributes data */
    p_device_ctx->identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
}

/**@brief Function for sending ON/OFF requests to the light bulb.
 *
 * @param[in]   param    Non-zero reference to ZigBee stack buffer that will be used to construct on/off request.
 * @param[in]   on_off   Requested state of the light bulb.
 */
static zb_void_t light_switch_identify_blink(zb_uint8_t param)
{
    zb_buf_t           * p_buf     = ZB_BUF_FROM_REF(param);
    zb_uint16_t          addr      = 0;

    NRF_LOG_INFO("Send Identify Trigger Effect Blink command");

    ZB_ZCL_IDENTIFY_SEND_TRIGGER_VARIANT_REQ_NO_APS_ACK(p_buf,
                        addr,
                        ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
                        0,
                        m_device_ctx.find_n_bind.current_ep,
                        ZB_AF_HA_PROFILE_ID,
                        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                        NULL,
                        ZB_ZCL_IDENTIFY_EFFECT_ID_BLINK,
                        ZB_ZCL_IDENTIFY_EFFECT_ID_VARIANT_DEFAULT);
}

/**@brief Function for switching to the next Light Switch endpoint.
 * 
 * @param[in]   next    Boolean to indicate should the next or previous endpoint from the list to be selected.
 */
static zb_void_t cycle_endpoint(zb_bool_t next)
{
    uint32_t error_code;
    zb_ret_t zb_err_code;

    /* Stop the timers which drive the indication LED. */
    error_code = app_timer_stop(m_led_indication_timer_aux);
    APP_ERROR_CHECK(error_code);
    error_code = app_timer_stop(m_led_indication_timer);
    APP_ERROR_CHECK(error_code);

    bsp_board_led_off(ENDPOINT_SELECTED_LED);

    static zb_uint8_t const ep_cycle_map[] = {LIGHT_SWITCH_ENDPOINT_I, LIGHT_SWITCH_ENDPOINT_II, LIGHT_SWITCH_ENDPOINT_III};
    static int ep_cycle_map_len = sizeof(ep_cycle_map)/sizeof(ep_cycle_map[0]);
    int idx = 0;

    while (ep_cycle_map[idx] != m_device_ctx.find_n_bind.current_ep)
    {
        idx++;
    }

    idx = next ? idx + 1 : idx - 1;
    
    if (idx == ep_cycle_map_len)
    {
        m_device_ctx.find_n_bind.current_ep = ep_cycle_map[0];
    }
    else if (idx == -1)
    {
        m_device_ctx.find_n_bind.current_ep = ep_cycle_map[ep_cycle_map_len - 1];
    }
    else
    {
        m_device_ctx.find_n_bind.current_ep = ep_cycle_map[idx];
    }

    NRF_LOG_INFO("Switched to the endpoint %d", m_device_ctx.find_n_bind.current_ep);

    /* Resume the indication LED procedure. */
    error_code = app_timer_start(m_led_indication_timer, APP_TIMER_TICKS(LED_ENDPOINT_INDICATION_OFF_TIME_MS), NULL);
    APP_ERROR_CHECK(error_code);

    zb_err_code = ZB_GET_OUT_BUF_DELAYED(light_switch_identify_blink);
    UNUSED_VARIABLE(zb_err_code);
}

/**@brief Callback to finding and binding procedure.
 *
 * @param[IN]   status  Procedure status.
 * @param[IN]   addr    Found device address.
 * @param[IN]   ep      Found device endpoint.
 * @param[IN]   cluster Common cluster ID.
 *
 * @return      Returned boolean value is used to decide if found device's cluster (ID given as parameter) should be bound.
 */
static zb_bool_t finding_n_binding_cb(zb_int16_t status, zb_ieee_addr_t addr, zb_uint8_t ep, zb_uint16_t cluster)
{
    zb_bool_t ret = ZB_FALSE;
    zb_char_t addr_buf[2 * 8 + 1];    /* 8 bytes (2 characters) plus one byte for null-terminator. */
    
    UNUSED_RETURN_VALUE(ieee_addr_to_str(addr_buf, sizeof(addr_buf), addr));

    switch (status)
    {
        case ZB_BDB_COMM_BIND_SUCCESS:
            NRF_LOG_INFO("Successfully bound node %s ep %hd cluster %hd",  NRF_LOG_PUSH(addr_buf), ep, cluster);
            break;

        case ZB_BDB_COMM_BIND_FAIL:
            NRF_LOG_INFO("Failed to bind node %s ep %hd cluster %hd", NRF_LOG_PUSH(addr_buf), ep, cluster);
            break;

        case ZB_BDB_COMM_BIND_ASK_USER:
            switch (cluster)
            {
                case ZB_ZCL_CLUSTER_ID_ON_OFF:
                case ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
                case ZB_ZCL_CLUSTER_ID_IDENTIFY:
                case ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
                    NRF_LOG_INFO("Trying to bind node %s ep %hd cluster %hd", NRF_LOG_PUSH(addr_buf), ep, cluster);
                    ret = ZB_TRUE;
                    break;
                default:
                    /* We are not interested in this cluster. */
                    break;
            }
            break;

        default:
            /* Should not happen */
            break;
    }

    return ret;
}

/**@brief Function for sending ON/OFF requests to the light bulb.
 *
 * @param[in]   param    Non-zero reference to ZigBee stack buffer that will be used to construct on/off request.
 * @param[in]   cmd_id   Type of the On/Off command.
 */
static zb_void_t light_switch_send_on_off(zb_uint8_t param, zb_uint16_t cmd_id)
{
    zb_buf_t           * p_buf     = ZB_BUF_FROM_REF(param);
    zb_uint16_t          addr      = 0;

    NRF_LOG_INFO("Send ON/OFF command: 0x%02x", cmd_id);

    ZB_ZCL_ON_OFF_SEND_REQ_NO_APS_ACK(p_buf,
                        addr,
                        ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
                        0,
                        m_device_ctx.find_n_bind.current_ep,
                        ZB_AF_HA_PROFILE_ID,
                        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                        (zb_uint8_t)cmd_id,
                        NULL);
}

/**@brief Function for getting a new Zigbee buffer and sending ON/OFF toggle request to the light bulb.
 *
 * @param[in]   p_context  Not used. Required by app_timer API.
 */
static void light_switch_send_delayed_toggle(void * p_context)
{
    zb_ret_t    zb_err_code;
    ret_code_t  err_code;

    UNUSED_PARAMETER(p_context);

    /* Request a buffer and call light_switch_send_toggle. */
    zb_err_code = ZB_GET_OUT_BUF_DELAYED2(light_switch_send_on_off, ZB_ZCL_CMD_ON_OFF_TOGGLE_ID);
    if (zb_err_code != RET_OK)
    {
        /* If there are no available buffers - reschedule toggle command. */
        err_code = app_timer_start(m_toggle_timer, APP_TIMER_TICKS(DELAYED_COMMAND_RETRY_MS), NULL);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for indicating the selected endpoint by the number of LED flashes.
 *
 * @param[in]   p_context  Not used. Required by app_timer API.
 */
static void led_indicate_endpoint(void * p_context)
{
    uint32_t error_code;
    uint32_t ticks = (1 + 2 * (m_device_ctx.find_n_bind.current_ep - 1));
    uint32_t delay = LED_ENDPOINT_INDICATION_ON_TIME_MS/ticks;

    UNUSED_PARAMETER(p_context);

    bsp_board_led_on(ENDPOINT_SELECTED_LED);

    error_code = app_timer_start(m_led_indication_timer_aux, APP_TIMER_TICKS(delay), (void *)ticks);
    APP_ERROR_CHECK(error_code);
}

/**@brief Function for indicating the selected endpoint by the number of LED flashes.
 *
 * @param[in]   p_context  Not used. Required by app_timer API.
 */
static void led_indicate_endpoint_cb(void * p_context)
{
    uint32_t error_code;
    uint32_t ticks_left = (uint32_t)p_context;
    uint32_t ticks = (1 + 2 * (m_device_ctx.find_n_bind.current_ep - 1));
    uint32_t delay = LED_ENDPOINT_INDICATION_ON_TIME_MS/ticks;

    bsp_board_led_invert(ENDPOINT_SELECTED_LED);

    if (--ticks_left != 0)
    {
        error_code = app_timer_start(m_led_indication_timer_aux, APP_TIMER_TICKS(delay), (void *)ticks_left);
        APP_ERROR_CHECK(error_code);
    }
}

/**@brief Function for sending step requests to the light bulb.
  *
  * @param[in]   param        Non-zero reference to ZigBee stack buffer that will be used to construct step request.
  * @param[in]   cmd_id       Command of the Level Cluster.
  */
static zb_void_t light_switch_send_step(zb_uint8_t param, zb_uint16_t cmd_id)
{
    zb_buf_t           * p_buf     = ZB_BUF_FROM_REF(param);
    zb_uint16_t          addr      = 0;

    NRF_LOG_INFO("Send step level command: 0x%02x", cmd_id);

    ZB_ZCL_LEVEL_CONTROL_SEND_STEP_REQ_NO_APS_ACK(p_buf,
                                    addr,
                                    ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
                                    0,
                                    m_device_ctx.find_n_bind.current_ep,
                                    ZB_AF_HA_PROFILE_ID,
                                    ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                                    NULL,
                                    (zb_uint8_t)cmd_id,
                                    LIGHT_SWITCH_DIM_STEP,
                                    LIGHT_SWITCH_DIM_TRANSITION_TIME);
}

/**@brief Function for sending move to hue saturation requests to the light bulb.
 *
 * @param[in]   param            Non-zero reference to ZigBee stack buffer that will be used to construct step request.
 * @param[in]   hue_saturation   Hue and saturation values squashed into a signle value. MSB represents hue, LSB represents saturation.
 */
static zb_void_t light_switch_send_move_to_hue_saturation(zb_uint8_t param, zb_uint16_t hue_saturation)
{
    zb_buf_t    * p_buf      = ZB_BUF_FROM_REF(param);
    zb_uint16_t   addr       = 0;
    zb_uint8_t    hue        = hue_saturation >> 8;
    zb_uint8_t    saturation = hue_saturation & 0x00FF;

    NRF_LOG_INFO("Send move to hue saturation command: hue = 0x%02x saturation = 0x%02x", hue, saturation);

    ZB_ZCL_COLOR_CONTROL_SEND_MOVE_TO_HUE_SATURATION_REQ_NO_APS_ACK(
            p_buf,
            addr,
            ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
            0,
            m_device_ctx.find_n_bind.current_ep,
            ZB_AF_HA_PROFILE_ID,
            ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
            NULL,
            hue,
            saturation,
            LIGHT_SWITCH_COLOR_TRANSITION_TIME);
}

/**@brief Perform local operation - leave network.
 *
 * @param[in]   param   Reference to ZigBee stack buffer that will be used to construct leave request.
 */
static void light_switch_leave_nwk(zb_uint8_t param)
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
        zb_err_code = ZB_GET_OUT_BUF_DELAYED(light_switch_leave_nwk);
        ZB_ERROR_CHECK(zb_err_code);
    }
}


/**@brief Function for starting join/rejoin procedure.
 *
 * param[in]   leave_type   Type of leave request (with or without rejoin).
 */
static zb_void_t light_switch_retry_join(zb_uint8_t leave_type)
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

static zb_void_t light_switch_leave_and_join(zb_uint8_t param)
{
    if (ZB_JOINED())
    {
        // Leave network. Joining procedure will be initiated inisde ZigBee stack signal handler.
        light_switch_leave_nwk(param);
    }
    else
    {
        // Already left network. Start joining procedure.
        light_switch_retry_join(ZB_NWK_LEAVE_TYPE_RESET);

        if (param)
        {
            ZB_FREE_BUF_BY_REF(param);
        }
    }
}

/**@brief Callback for detecting button press duration.
 *
 * @param[in]   button   BSP Button that was pressed.
 */
static zb_void_t light_switch_button_handler(zb_uint8_t button)
{
    zb_time_t   current_time;
    zb_bool_t   short_expired;
    zb_uint16_t cmd_id;
    zb_ret_t    zb_err_code;

    current_time = ZB_TIMER_GET();

    if (ZB_TIME_SUBTRACT(current_time, m_device_ctx.button.timestamp) > LIGHT_SWITCH_BUTTON_THRESHOLD)
    {
        short_expired = ZB_TRUE;
    }
    else
    {
        short_expired = ZB_FALSE;
    }

    /* Check if button was released during LIGHT_SWITCH_BUTTON_SHORT_POLL_TMO. */
    if (!bsp_button_is_pressed(button))
    {
        if (!short_expired)
            {
                /* Allocate output buffer and send on/off command. */
            cmd_id = button == LIGHT_SWITCH_BUTTON_ON ? ZB_ZCL_CMD_ON_OFF_ON_ID : ZB_ZCL_CMD_ON_OFF_OFF_ID;
            zb_err_code = ZB_GET_OUT_BUF_DELAYED2(light_switch_send_on_off, cmd_id);
                if (zb_err_code == RET_ERROR)
                {
                    NRF_LOG_WARNING("No frame sent - out of buffers. Wait a bit.");
                }
                else
                {
                    ZB_ERROR_CHECK(zb_err_code);
                }
            }

        /* Button released - wait for accept next event. */
        m_device_ctx.button.in_progress = ZB_FALSE;
    }
    else
    {
        if (short_expired)
        {
            /* The button is still pressed - allocate output buffer and send step command. */
            cmd_id = button == LIGHT_SWITCH_BUTTON_ON ? ZB_ZCL_LEVEL_CONTROL_STEP_MODE_UP : ZB_ZCL_LEVEL_CONTROL_STEP_MODE_DOWN;
            zb_err_code = ZB_GET_OUT_BUF_DELAYED2(light_switch_send_step, cmd_id);
            if (zb_err_code == RET_ERROR)
            {
                NRF_LOG_WARNING("No frame sent - out of buffers. Wait a bit.");
            }
            else
            {
                ZB_ERROR_CHECK(zb_err_code);
            }
            zb_err_code = ZB_SCHEDULE_ALARM(light_switch_button_handler, button, LIGHT_SWITCH_BUTTON_LONG_POLL_TMO);
            ZB_ERROR_CHECK(zb_err_code);
        }
        else
        {
            /* Wait another LIGHT_SWITCH_BUTTON_SHORT_POLL_TMO, until LIGHT_SWITCH_BUTTON_THRESHOLD will be reached. */
            zb_err_code = ZB_SCHEDULE_ALARM(light_switch_button_handler, button, LIGHT_SWITCH_BUTTON_SHORT_POLL_TMO);
            ZB_ERROR_CHECK(zb_err_code);
        }
    }
}

/**@brief Function to start the Finding & Binding Procedure.
 *        If the Finding & Binding Procedure was already started, cancel it.
 */
static zb_void_t toggle_find_n_bind(void)
{
    zb_ret_t zb_err_code;

    zb_err_code = zb_bdb_finding_binding_initiator(m_device_ctx.find_n_bind.current_ep, finding_n_binding_cb);
    if (zb_err_code == RET_OK)
    {
        bsp_board_led_on(FINDING_N_BINDING_STATE_LED);
        NRF_LOG_INFO("F&B: Started Finding & Binding procedure on the endpoint %d.", m_device_ctx.find_n_bind.current_ep);
    }
    else if (zb_err_code == RET_BUSY)
    {
        zb_bdb_finding_binding_initiator_cancel();
    }
    else if (zb_err_code == RET_INVALID_STATE)
    {
        NRF_LOG_WARNING("Device not yet commissionned!");
    }
    else
    {
        ZB_ERROR_CHECK(zb_err_code);
    }
}

/**@brief Callback for button events.
 *
 * @param[in]   evt      Incoming event from the BSP subsystem.
 */
static void buttons_handler(bsp_event_t evt)
{
    zb_ret_t   zb_err_code;
    zb_int32_t button;

    switch(evt)
    {
        case BSP_EVENT_KEY_0:
            button = LIGHT_SWITCH_BUTTON_ON;
            break;

        case BSP_EVENT_KEY_1:
            button = LIGHT_SWITCH_BUTTON_OFF;
            break;

        case BSP_EVENT_KEY_2:
            cycle_endpoint(ZB_TRUE);
            return;

        case BSP_EVENT_KEY_3:
            toggle_find_n_bind();
            return;

#ifdef ARDUINO_JOYSTICK_SHIELD_V1A
        case BSP_EVENT_KEY_4:
            cycle_endpoint(ZB_FALSE);
            return;
#endif

        default:
            NRF_LOG_INFO("Unhandled BSP Event received: %d", evt);
            return;
    }

    if (!m_device_ctx.button.in_progress)
    {
        m_device_ctx.button.in_progress = ZB_TRUE;
        m_device_ctx.button.timestamp = ZB_TIMER_GET();

        zb_err_code = ZB_SCHEDULE_ALARM(light_switch_button_handler, button, LIGHT_SWITCH_BUTTON_SHORT_POLL_TMO);
        ZB_ERROR_CHECK(zb_err_code);
    }
}


/**@brief Function for initializing LEDs and buttons.
 */
static void leds_buttons_init(void)
{
    ret_code_t error_code;

    /* Initialize LEDs and buttons - use BSP to control them. */
    error_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, buttons_handler);
    APP_ERROR_CHECK(error_code);
    /* By default the bsp_init attaches BSP_KEY_EVENTS_{0-4} to the PUSH events of the corresponding buttons. */

    bsp_board_leds_off();
}

/**@brief Function to set the Sleeping Mode according to the SLEEPY_ON_BUTTON state.
*/
static zb_void_t sleepy_device_setup(void)
{
    zb_set_rx_on_when_idle(bsp_button_is_pressed(SLEEPY_ON_BUTTON) ? ZB_FALSE : ZB_TRUE);
}

#ifdef ARDUINO_JOYSTICK_SHIELD_V1A
/**@brief Callback to run when the joystick-based values of hue and saturation have been calculated. */
static zb_void_t joystick_cb (void * p_context)
{
    zb_uint32_t hue_saturation = (zb_uint32_t) p_context;

    if (ZB_JOINED())
    {
        UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_move_to_hue_saturation, (zb_uint16_t)hue_saturation));
    }
}
#endif

/**@brief Function for initializing the Zigbee Stack
 */
static void zigbee_init(void)
{
    zb_ieee_addr_t ieee_addr;

    /* Set ZigBee stack logging level and traffic dump subsystem. */
    ZB_SET_TRACE_LEVEL(ZIGBEE_TRACE_LEVEL);
    ZB_SET_TRACE_MASK(ZIGBEE_TRACE_MASK);
    ZB_SET_TRAF_DUMP_OFF();

    /* Initialize ZigBee stack. */
    ZB_INIT("light_switch_nus");

    /* Set device address to the value read from FICR registers. */
    zb_osif_get_ieee_eui64(ieee_addr);
    zb_set_long_address(ieee_addr);

    /* Set up Zigbee protocol main parameters. */
    zb_set_network_ed_role(IEEE_CHANNEL_MASK);
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);

    zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));
    sleepy_device_setup();

    /* Register dimmer switch device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&color_dimmer_switch_ctx);

    /* Initialize cluster's attributes. */
    light_switch_clusters_attr_init(&zb_dev_ctx_first);
    light_switch_clusters_attr_init(&zb_dev_ctx_second);
    light_switch_clusters_attr_init(&zb_dev_ctx_third);

    /* Set the current endpoint to the first one */
    m_device_ctx.find_n_bind.current_ep = LIGHT_SWITCH_ENDPOINT_I;
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
    uint32_t                       error_code;
    zb_ret_t                       zb_err_code;

    switch(sig)
    {
        case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (status == RET_OK)
            {
                NRF_LOG_INFO("Joined network successfully");
                bsp_board_led_on(ZIGBEE_NETWORK_STATE_LED);

                error_code = app_timer_start(m_led_indication_timer, APP_TIMER_TICKS(LED_ENDPOINT_INDICATION_OFF_TIME_MS), NULL);
                APP_ERROR_CHECK(error_code);
            }
            else
            {
                NRF_LOG_ERROR("Failed to join network. Status: %d", status);
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);
                zb_err_code = ZB_SCHEDULE_ALARM(light_switch_leave_and_join, 0, ZB_TIME_ONE_SECOND);
                ZB_ERROR_CHECK(zb_err_code);
            }
            break;

        case ZB_ZDO_SIGNAL_LEAVE:
            if (status == RET_OK)
            {
                bsp_board_led_off(ZIGBEE_NETWORK_STATE_LED);
                p_leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_leave_params_t);
                NRF_LOG_INFO("Network left. Leave type: %d", p_leave_params->leave_type);
                light_switch_retry_join(p_leave_params->leave_type);
            }
            else
            {
                NRF_LOG_ERROR("Unable to leave network. Status: %d", status);
            }
            break;

        case ZB_COMMON_SIGNAL_CAN_SLEEP:
            {
                zb_zdo_signal_can_sleep_params_t * can_sleep_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_can_sleep_params_t);
                NRF_LOG_INFO("Can sleep for %ld ms", can_sleep_params->sleep_tmo);
                zb_sleep_now();
            }
            break;

        case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
            {
                zb_zdo_signal_fb_initiator_finished_params_t * f_n_b_status = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_fb_initiator_finished_params_t);
                switch(f_n_b_status->status)
                {
                    case ZB_ZDO_FB_INITIATOR_STATUS_SUCCESS:
                        NRF_LOG_INFO("F&B: Remote peer has been bound.");
                        break;

                    case ZB_ZDO_FB_INITIATOR_STATUS_CANCEL:
                        NRF_LOG_INFO("F&B: Initiator process was cancelled.");
                        break;

                    case ZB_ZDO_FB_INITIATOR_STATUS_ALARM:
                        NRF_LOG_INFO("F&B: Initiator process was timed out.");
                        break;

                    case ZB_ZDO_FB_INITIATOR_STATUS_ERROR:
                        NRF_LOG_ERROR("F&B: Error.");
                        break;

                    default:
                        NRF_LOG_ERROR("F&B: Unknown error, status %d.", f_n_b_status->status);
                        break;
                }
            }
            bsp_board_led_off(FINDING_N_BINDING_STATE_LED);
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

static void hue_saturation_cmd(int count, uint32_t h, uint32_t s)
{
    UNUSED_PARAMETER(count);
    int32_t   hue = h & 0xFF;
    int32_t   saturation = s & 0xFF;

    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_move_to_hue_saturation, (hue << 8) | saturation));
}

static void finding_n_binding_cmd(int count)
{
    UNUSED_PARAMETER(count);
    toggle_find_n_bind();
}

static void turn_on_cmd(int count)
{
    UNUSED_PARAMETER(count);
    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_on_off, ZB_ZCL_CMD_ON_OFF_ON_ID));
}

static void turn_off_cmd(int count)
{
    UNUSED_PARAMETER(count);
    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_on_off, ZB_ZCL_CMD_ON_OFF_OFF_ID));
}

static void toggle_cmd(int count)
{
    UNUSED_PARAMETER(count);
    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_on_off, ZB_ZCL_CMD_ON_OFF_TOGGLE_ID));
}

static void increase_cmd(int count)
{
    UNUSED_PARAMETER(count);
    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_step, ZB_ZCL_LEVEL_CONTROL_STEP_MODE_UP));
}

static void decrease_cmd(int count)
{
    UNUSED_PARAMETER(count);
    UNUSED_RETURN_VALUE(ZB_GET_OUT_BUF_DELAYED2(light_switch_send_step, ZB_ZCL_LEVEL_CONTROL_STEP_MODE_DOWN));
}

static void next_ep_cmd(int count)
{
    UNUSED_PARAMETER(count);
    cycle_endpoint(ZB_TRUE);
}

static void prev_ep_cmd(int count)
{
    UNUSED_PARAMETER(count);
    cycle_endpoint(ZB_FALSE);
}

static void turn_delay_cmd(int count, uint32_t delay)
{
    UNUSED_PARAMETER(count);
    uint32_t err_code;

    if (delay > 999)
    {
            NRF_LOG_INFO("Delay value out of range 0-999:");
            return;
    }

    /* Cancel previous delayed toggle command. */
    err_code = app_timer_stop(m_toggle_timer);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("Schedule delay: %d", delay);

    /* Check if delay is not too short. */
    if (APP_TIMER_TICKS(delay * 1000LL) < 5)
    {
        light_switch_send_delayed_toggle(NULL);
        return;
    }

    /* Start toggle timer. */
    err_code = app_timer_start(m_toggle_timer, APP_TIMER_TICKS(delay * 1000LL), NULL);
    APP_ERROR_CHECK(err_code);
}

static void add_nus_commands(void)
{
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_ON, turn_on_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_OFF, turn_off_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_TOGGLE, toggle_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_INCREASE, increase_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_DECREASE, decrease_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_SWITCH_NEXT_EP, next_ep_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_SWITCH_PREV_EP, prev_ep_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_HUE_SATURATION, hue_saturation_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_FIND_N_BIND, finding_n_binding_cmd));
    UNUSED_RETURN_VALUE(NUS_ADD_COMMAND(COMMAND_TOGGLE_DELAY, turn_delay_cmd));
}

/***************************************************************************************************
* @section Main
**************************************************************************************************/


/**@brief Function for application main entry.
 */
int main(void)
{
    zb_ret_t   zb_err_code;

    /* Initialize loging system and GPIOs. */
    log_init();
    timer_init();
    leds_buttons_init();

    /* Bluetooth initialization. */
    ble_stack_init();
    /* NUS Commander initialization. */
    nus_init(NULL);
    
    /* Add commands to NUS */
    add_nus_commands();

    /* Initialize Zigbee stack. */
    zigbee_init();

#ifdef ARDUINO_JOYSTICK_SHIELD_V1A
    /* Initialize the Joystick routines. */
    joystick_init(joystick_cb);
#endif

    /* Start execution. */
    NRF_LOG_INFO("BLE Zigbee dynamic light switch example started.");

    /** Start Zigbee Stack. */
    zb_err_code = zboss_start();
    ZB_ERROR_CHECK(zb_err_code);

    while(1)
    {
        zboss_main_loop_iteration();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    }
}


/**
 * @}
 */
