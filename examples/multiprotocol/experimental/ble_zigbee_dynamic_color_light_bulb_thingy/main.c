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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy main.c
 * @{
 * @ingroup zigbee_examples
 * @brief Dynamic multiprotocol example application to demonstrate control on BLE device (peripheral role) using zigbee device.
 */

#include "zboss_api.h"
#include "zb_mem_config_min.h"
#include "zb_error_handler.h"
#include "zigbee_color_light.h"
#include "zigbee_helpers.h"
#include "ble_thingy_master.h"
#include "nrf_pwr_mgmt.h"
#include "rgb_led.h"
#include "app_timer.h"
#include "bsp.h"

/* ZigBee device configuration values. */
#define ERASE_PERSISTENT_CONFIG           ZB_FALSE                  /**< Do not erase NVRAM to save the network parameters after device reboot or power-off. */
#define THINGY_DEVICE_CNT                 2                         /**< Amount of Thingy devices to connect to. */

#define ZB_NETWORK_STATE_LED              BSP_BOARD_LED_2           /**< LED indicating that device successfully joined ZigBee network. */
#define ZB_ONGOING_FIND_N_BIND_LED        BSP_BOARD_LED_3           /**< LED to indicate ongoing find and bind procedure. */
#define SLEEPY_ON_BUTTON                  BSP_BOARD_BUTTON_2        /**< Button ID used to determine if we need the sleepy device behaviour at start (pressed means yes). */
#define ZB_TOGGLE_FIND_N_BIND_BSP_EVT     BSP_EVENT_KEY_3           /**< Button event used to toggle identifying mode on a local endpoint. */
#define ZB_KEEP_ALIVE_TIMEOUT             500                       /**< ZED data poll period, in milliseconds. */
#define ZB_USE_LEGACY_MODE                0                         /**< Set to 1 to disable TC Link Key change */

#define ENDPOINT_IDENTIFY_TIME            (ZB_TIME_ONE_SECOND * 30) /**< Thingy device identification time. */
#define HA_COLOR_LIGHT_ENDPOINT_1_ID      10                        /**< Device first endpoint, used to receive light controlling commands. */
#define HA_COLOR_LIGHT_ENDPOINT_2_ID      11                        /**< Device second endpoint, used to receive light controlling commands. */
#define HA_COLOR_LIGHT_ENDPOINT_3_ID      12                        /**< Device third endpoint, used to receive light controlling commands. */

#define IEEE_CHANNEL_MASK                 (1l << ZIGBEE_CHANNEL)    /**< Scan only one, predefined channel to find the coordinator. */

/* Structure to store zigbee endpoint related variables */
typedef struct
{
    zb_color_light_ctx_t * const p_light_ctx; /**< Pointer to structure containing light context. */
    thingy_device_t      * p_thingy;          /**< Pointer to Thingy context . */
} light_thingy_ctx_t;

/* Declare thingy device list with device count store in APP_BLE_THINGY_DEVICE_CNT */
static thingy_device_t m_thingy_dev[THINGY_DEVICE_CNT];

/* Declare context variable and cluster attribute list for first endpoint */
zb_color_light_ctx_t m_color_light_ctx_1;
zb_color_light_ctx_t m_color_light_ctx_2;
zb_color_light_ctx_t m_color_light_ctx_3;

ZB_DECLARE_COLOR_CONTROL_CLUSTER_ATTR_LIST(m_color_light_ctx_1,
                                           m_color_light_clusters_1);

/* Declare context variable and cluster attribute list for second endpoint */
ZB_DECLARE_COLOR_CONTROL_CLUSTER_ATTR_LIST(m_color_light_ctx_2,
                                           m_color_light_clusters_2);

/* Declare context variable and cluster attribute list for third endpoint */
ZB_DECLARE_COLOR_CONTROL_CLUSTER_ATTR_LIST(m_color_light_ctx_3,
                                           m_color_light_clusters_3);

/* Declare two endpoints for color controllable and dimmable light bulbs */
ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_EP(m_color_light_ep_1,
                                       HA_COLOR_LIGHT_ENDPOINT_1_ID,
                                       m_color_light_clusters_1);

ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_EP(m_color_light_ep_2,
                                       HA_COLOR_LIGHT_ENDPOINT_2_ID,
                                       m_color_light_clusters_2);

ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_EP(m_color_light_ep_3,
                                       HA_COLOR_LIGHT_ENDPOINT_3_ID,
                                       m_color_light_clusters_3);

/* Declare context for endpoints */
ZBOSS_DECLARE_DEVICE_CTX_3_EP(m_color_light_ctx,
                              m_color_light_ep_1,
                              m_color_light_ep_2,
                              m_color_light_ep_3);

/* An array to store information about Zigbee endpoints and associated
 * Thingy:52 devices. The third Endpoint represents a local RGB light.
 */
static light_thingy_ctx_t m_light_thingy_ctx[] =
{
    {
        .p_light_ctx = &m_color_light_ctx_1,
        .p_thingy = &m_thingy_dev[0]
    },
    {
        .p_light_ctx = &m_color_light_ctx_2,
        .p_thingy = &m_thingy_dev[1]
    },
    {
        .p_light_ctx = &m_color_light_ctx_3,
        .p_thingy = NULL
    }
};

/**@brief Function to get pointer to device context by zigbee endpoint ID
 *
 * @param[IN] ep  Zigbee endpoint ID.
 *
 * @returns Pointer to matching device context or NULL if context not found.
 */
static light_thingy_ctx_t * find_ctx_by_ep_id(zb_uint8_t ep)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(m_light_thingy_ctx); i++)
    {
        if (ep == m_light_thingy_ctx[i].p_light_ctx->ep_id)
        {
            return &m_light_thingy_ctx[i];
        }
    }

    return NULL;
}

/**@brief Find device context by a pointer to Thingy object.
 *
 * @param[IN] p_thingy  Pointer to the Thingy object.
 *
 * @returns Pointer to matching device context
 */
static light_thingy_ctx_t * find_ctx_by_thingy(const thingy_device_t * p_thingy)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(m_light_thingy_ctx); i++)
    {
        if (p_thingy == m_light_thingy_ctx[i].p_thingy)
        {
            return &m_light_thingy_ctx[i];
        }
    }

    return NULL;
}

/**@brief Function to update LED state on device using given parameters.
 *
 * @param[IN]  ep            Endpoint ID for which LED state should be updated.
 * @param[IN]  p_led_params  Pointer to structure containing LED parameters.
 */
void update_endpoint_led(zb_uint8_t ep, led_params_t * p_led_params)
{
    light_thingy_ctx_t * p_ctx = find_ctx_by_ep_id(ep);

    if (p_ctx->p_thingy)
    {
        ble_thingy_master_update_led(p_ctx->p_thingy, p_led_params);
    }
    else
    {
        rgb_led_update(p_led_params);
    }
}

/**@brief Function to handle identify notification events on endpoint.
 *
 * @param[IN] param Parameter handler is called with.
 * @param[in] ep    Endpoint ID
 */
static zb_void_t zb_identify_ep_handler(zb_uint8_t param, zb_uint16_t ep)
{
    light_thingy_ctx_t * p_ctx = find_ctx_by_ep_id(ep);
    zb_ret_t             ret   = RET_OK;

    if (p_ctx)
    {
        NRF_LOG_INFO("Endpoint %d, param value: %hd", ep, param);

        if (param)
        {
            /* Turn on led indicating ongoing find and bind procedure and set Thingy
             * LED to breathing green to indicate ongoing procedure. */
            bsp_board_led_on(ZB_ONGOING_FIND_N_BIND_LED);
            ret = zb_color_light_do_identify_effect(p_ctx->p_light_ctx,
                                                    ZB_ZCL_IDENTIFY_EFFECT_ID_BREATHE);
        }
        else
        {
            /* Turn off led indicating ongoing find and bind procedure and
             * restore Thingy LED color. */
            bsp_board_led_off(ZB_ONGOING_FIND_N_BIND_LED);
            ret = zb_color_light_do_identify_effect(p_ctx->p_light_ctx,
                                                    ZB_ZCL_IDENTIFY_EFFECT_ID_STOP);
        }
    }
    else
    {
        NRF_LOG_INFO("Can not find matching device.");
    }

    UNUSED_RETURN_VALUE(ret);
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param[IN]   param   Parameter handler is called with.
 */
static zb_void_t zb_identify_ep_1_handler(zb_uint8_t param)
{
    zb_identify_ep_handler(param, HA_COLOR_LIGHT_ENDPOINT_1_ID);
}

/**@brief Function to handle identify notification events on the second endpoint.
 *
 * @param[IN]   param   Parameter handler is called with.
 */
static zb_void_t zb_identify_ep_2_handler(zb_uint8_t param)
{
    zb_identify_ep_handler(param, HA_COLOR_LIGHT_ENDPOINT_2_ID);
}

/**@brief Function to handle identify notification events on the third endpoint.
 *
 * @param[IN]   param   Parameter handler is called with.
 */
static zb_void_t zb_identify_ep_3_handler(zb_uint8_t param)
{
    zb_identify_ep_handler(param, HA_COLOR_LIGHT_ENDPOINT_3_ID);
}

/**@brief Function to toggle start/stop find and bind on Endpoint.
 *
 * @param[IN] ep  Endpoint ID.
 */
static zb_void_t zb_toggle_find_n_bind(uint8_t ep)
{
    // There is no API to check f & b state, so we need to store the state.
    static zb_bool_t f_n_b_in_progess[3] = {ZB_FALSE};
    const int c_idx = ep - HA_COLOR_LIGHT_ENDPOINT_1_ID;

    if (f_n_b_in_progess[c_idx] == ZB_FALSE)
    {
        zb_ret_t zb_err_code;
        zb_err_code = zb_bdb_finding_binding_target(ep);
        ZB_ERROR_CHECK(zb_err_code);
    }
    else
    {
        zb_bdb_finding_binding_target_cancel();
    }

    f_n_b_in_progess[c_idx] = f_n_b_in_progess[c_idx] ? ZB_FALSE : ZB_TRUE;
}

/**@brief Perform local operation - leave network.
 *
 * @param[IN]   param   Reference to ZigBee stack buffer that will be used to construct leave request.
 */
static void zb_leave_nwk(zb_uint8_t param)
{
    zb_ret_t                    zb_err_code;
    zb_buf_t                  * p_buf = ZB_BUF_FROM_REF(param);
    zb_zdo_mgmt_leave_param_t * p_req_param;

    p_req_param = ZB_GET_BUF_PARAM(p_buf, zb_zdo_mgmt_leave_param_t);
    UNUSED_RETURN_VALUE(ZB_BZERO(p_req_param, sizeof(zb_zdo_mgmt_leave_param_t)));

    /* We are going to leave */
    if (param)
    {
        /* Set dst_addr == local address for local leave */
        p_req_param->dst_addr = ZB_PIBCACHE_NETWORK_ADDRESS();
        p_req_param->rejoin   = ZB_FALSE;
        UNUSED_RETURN_VALUE(zdo_mgmt_leave_req(param, NULL));
    }
    else
    {
        zb_err_code = ZB_GET_OUT_BUF_DELAYED(zb_leave_nwk);
        ZB_ERROR_CHECK(zb_err_code);
    }
}

/**@brief Function for starting join/rejoin procedure.
 *
 * param[IN]   leave_type   Type of leave request (with or without rejoin).
 */
static zb_void_t zb_retry_join(zb_uint8_t leave_type)
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
 * @param[IN]   param   Optional reference to ZigBee stack buffer to be reused by leave and join procedure.
 */
static zb_void_t zb_leave_and_join(zb_uint8_t param)
{
    if (ZB_JOINED())
    {
        /* Leave network. Joining procedure will be initiated inisde ZigBee stack signal handler. */
        zb_leave_nwk(param);
    }
    else
    {
        /* Already left network. Start joining procedure. */
        zb_retry_join(ZB_NWK_LEAVE_TYPE_RESET);

        if (param)
        {
            ZB_FREE_BUF_BY_REF(param);
        }
    }
}


/**@brief Callback function for handling ZCL commands.
 *
 * @param[IN]   param   Reference to ZigBee stack buffer used to pass received data.
 */
static zb_void_t zb_zcl_device_cb(zb_uint8_t param)
{
    zb_buf_t                        * p_buffer          = ZB_BUF_FROM_REF(param);
    zb_zcl_device_callback_param_t  * p_device_cb_param = ZB_GET_BUF_PARAM(p_buffer, zb_zcl_device_callback_param_t);
    light_thingy_ctx_t              * p_light_thingy_ctx;
    zb_ret_t                          ret = RET_OK;

    NRF_LOG_INFO("Received ZCL callback %hd on endpoint %hu",
                 p_device_cb_param->device_cb_id, p_device_cb_param->endpoint);

    p_light_thingy_ctx = find_ctx_by_ep_id(p_device_cb_param->endpoint);
    if (!p_light_thingy_ctx)
    {
        NRF_LOG_WARNING("Context for endpoint %hu not found", p_device_cb_param->endpoint);
        return;
    }

    /* Prevent led update if related Endpoint is in identify mode. */
    if (!p_light_thingy_ctx->p_light_ctx->identify_attr.identify_time)
    {
        switch (p_device_cb_param->device_cb_id)
        {
            case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID:
                ret = zb_color_light_set_level(p_light_thingy_ctx->p_light_ctx,
                                               p_device_cb_param->cb_param.level_control_set_value_param.new_value);
                break;

            case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
                ret = zb_color_light_set_attribute(p_light_thingy_ctx->p_light_ctx,
                                                   &p_device_cb_param->cb_param.set_attr_value_param);
                break;

            case ZB_ZCL_IDENTIFY_EFFECT_CB_ID:
                ret = zb_color_light_do_identify_effect(p_light_thingy_ctx->p_light_ctx,
                                                        p_device_cb_param->cb_param.identify_effect_value_param.effect_id);
                break;

            default:
                ret = RET_ERROR;
                NRF_LOG_INFO("Default case, returned error");
                break;
        }
    }
    else
    {
        NRF_LOG_INFO("Can't update LED, endpoint in identify mode");
        ret = RET_ERROR;
    }

    /* Set default response value. */
    p_device_cb_param->status = ret;
    NRF_LOG_INFO("zcl_device_cb status: %hd", p_device_cb_param->status);
}

/**@brief ZigBee stack event handler.
 *
 * @param[IN] param   Reference to ZigBee stack buffer used to pass arguments (signal).
 */
void zboss_signal_handler(zb_uint8_t param)
{
    zb_zdo_app_signal_hdr_t       * p_sg_p          = NULL;
    zb_zdo_signal_leave_params_t  * p_leave_params  = NULL;
    zb_zdo_app_signal_type_t        sig             = zb_get_app_signal(param, &p_sg_p);
    zb_ret_t                        status          = ZB_GET_APP_SIGNAL_STATUS(param);
    zb_ret_t                        zb_err_code     = 0;

    switch (sig)
    {
        case ZB_BDB_SIGNAL_DEVICE_REBOOT:
            /* fall-through */
        case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
            if (status == RET_OK)
            {
                NRF_LOG_INFO("Joined network successfully");
                bsp_board_led_on(ZB_NETWORK_STATE_LED);
                ble_thingy_master_scan(APP_BLE_THINGY_SCANNING_TIMEOUT);
            }
            else
            {
                NRF_LOG_ERROR("Failed to join network. Status: %d", status);
                bsp_board_led_off(ZB_NETWORK_STATE_LED);
                zb_err_code = ZB_SCHEDULE_ALARM(zb_leave_and_join, 0, ZB_TIME_ONE_SECOND);
                ZB_ERROR_CHECK(zb_err_code);
            }
            break;

        case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
            if (status != RET_OK)
            {
                NRF_LOG_WARNING("Production config is not present or invalid");
            }
            break;

        case ZB_ZDO_SIGNAL_LEAVE:
            if (status == RET_OK)
            {
                bsp_board_led_off(ZB_NETWORK_STATE_LED);
                p_leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_leave_params_t);
                NRF_LOG_INFO("Network left. Leave type: %d", p_leave_params->leave_type);
                zb_retry_join(p_leave_params->leave_type);
            }
            else
            {
                NRF_LOG_ERROR("Unable to leave network. Status: %d", status);
            }
            break;

        case ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED:
            NRF_LOG_INFO("Find and bind target finished, status: %d", status);
            break;

        case ZB_COMMON_SIGNAL_CAN_SLEEP:
            {
                zb_sleep_now();
            }
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


/**@brief Function to handle Thingy's events.
 *
 * @param[IN] p_thingy  Pointer to the Thingy device structure.
 * @param[IN] evt       Event from the specified Thingy device.
 */
static void thingy_event_handler(thingy_device_t * p_thingy, thingy_evt_t evt)
{
    light_thingy_ctx_t * p_ctx = find_ctx_by_thingy(p_thingy);

    if (p_ctx == NULL)
    {
        return;
    }

    switch (evt)
    {
        case THINGY_BUTTON_PRESSED:
            zb_toggle_find_n_bind(p_ctx->p_light_ctx->ep_id);
            break;

        case THINGY_BUTTON_RELEASED:
            break;

        case THINGY_CONNECTED:
        case THINGY_DISCONNECTED:
            p_thingy->frame_lock = (evt == THINGY_CONNECTED) ? true : false;
            ble_thingy_master_scan(APP_BLE_THINGY_SCANNING_TIMEOUT);
            break;

        default:
            break;
    }
}

/**@brief Function to handle button events.
 *
 * @param[IN] evt  Event from pressed button.
 */
static zb_void_t buttons_handler(bsp_event_t evt)
{
    switch (evt)
    {
        case ZB_TOGGLE_FIND_N_BIND_BSP_EVT:
            zb_toggle_find_n_bind(m_color_light_ctx_3.ep_id);
            break;

        default:
            break;
    }
}

/**@brief Function initializing leds and buttons used by application. */
static zb_void_t leds_buttons_init(void)
{
    ret_code_t err_code;
    /* Initialize LEDs and buttons */
    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, buttons_handler);
    APP_ERROR_CHECK(err_code);
    /* By default the bsp_init attaches BSP_KEY_EVENTS_{0-4} to the PUSH events of the corresponding buttons. */
}

/**@brief Function for initializing the log. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing the Power management. */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function to set the Sleeping Mode according to the SLEEPY_ON_BUTTON state.
*/
static zb_void_t sleepy_device_setup(void)
{
    zb_set_rx_on_when_idle(bsp_button_is_pressed(SLEEPY_ON_BUTTON) ? ZB_FALSE : ZB_TRUE);
}


/**@brief Function for application main entry. */
int main(void)
{
    /* Declare variables to initialize ZED address */
    zb_ret_t    zb_err_code;
    ret_code_t  err_code;

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    log_init();
    leds_buttons_init();
    power_management_init();
    rgb_led_init();

    err_code = ble_thingy_master_init(m_thingy_dev,
                                      THINGY_DEVICE_CNT,
                                      thingy_event_handler);
    APP_ERROR_CHECK(err_code);

    /* Set ZigBee stack logging level and traffic dump subsystem. */
    ZB_SET_TRACE_LEVEL(ZIGBEE_TRACE_LEVEL);
    ZB_SET_TRACE_MASK(ZIGBEE_TRACE_MASK);
    ZB_SET_TRAF_DUMP_OFF();

    /* Initialize ZigBee stack. */
    ZB_INIT("color_dimmable_light");

    /* Read long address from FICR. */
    zb_ieee_addr_t m_ieee_addr;
    zb_osif_get_ieee_eui64(m_ieee_addr);
    zb_set_long_address(m_ieee_addr);

    /* Set device role */
    #if (ZB_USE_LEGACY_MODE == 1)
    zb_set_network_ed_role_legacy(IEEE_CHANNEL_MASK);
    #else
    zb_set_network_ed_role(IEEE_CHANNEL_MASK);
    #endif

    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ZB_KEEP_ALIVE_TIMEOUT));
    zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    sleepy_device_setup();

    // Register device context with ZBOSS prior to using zb_color_light
    // functions as the module uses ZBOSS APIs which operate on the device object.
    ZB_AF_REGISTER_DEVICE_CTX(&m_color_light_ctx);
    ZB_ZCL_REGISTER_DEVICE_CB(zb_zcl_device_cb);

    zb_color_light_init();

    zb_color_light_init_ctx(&m_color_light_ctx_1,
                            HA_COLOR_LIGHT_ENDPOINT_1_ID,
                            zb_identify_ep_1_handler);

    zb_color_light_init_ctx(&m_color_light_ctx_2,
                            HA_COLOR_LIGHT_ENDPOINT_2_ID,
                            zb_identify_ep_2_handler);

    zb_color_light_init_ctx(&m_color_light_ctx_3,
                            HA_COLOR_LIGHT_ENDPOINT_3_ID,
                            zb_identify_ep_3_handler);

    m_color_light_ctx_3.value_debounce_time = 0;


    /* Start Zigbee Stack. */
    zb_err_code = zboss_start();
    ZB_ERROR_CHECK(zb_err_code);

    /* Enter main loop */
    for (;;)
    {
        zboss_main_loop_iteration();
        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    }
}

/**
 * @}
 */
