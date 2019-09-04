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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy zigbee_color_light.c
 * @{
 * @ingroup zigbee_examples
 */
#include <stdbool.h>
#include <stdint.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "zboss_api.h"
#include "zb_zcl_color_control.h"
#include "zb_error_handler.h"
#include "zigbee_color_light.h"

#define LIGHT_LOCATION_KITCHEN              0x1D
#define LIGHT_LOCATION_OFFICE               0x24

/* Basic cluster attributes initial values. */
#define BULB_INIT_BASIC_APP_VERSION         01                                  /**< Version of the application software (1 byte). */
#define BULB_INIT_BASIC_STACK_VERSION       10                                  /**< Version of the implementation of the ZigBee stack (1 byte). */
#define BULB_INIT_BASIC_HW_VERSION          11                                  /**< Version of the hardware of the device (1 byte). */
#define BULB_INIT_BASIC_MANUF_NAME          "Nordic Semiconductor"              /**< Manufacturer name (32 bytes). */
#define BULB_INIT_BASIC_MODEL_ID            "Color_Dimmable_Light_Bulb_v0.1"    /**< Model number assigned by manufacturer (32-bytes long string). */
#define BULB_INIT_BASIC_DATE_CODE           "20180718"                          /**< First 8 bytes specify the date of manufacturer of the device in ISO 8601 format (YYYYMMDD). Th rest (8 bytes) are manufacturer specific. */
#define BULB_INIT_BASIC_POWER_SOURCE        ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE /**< Type of power sources available for the device. For possible values see section 3.2.2.2.8 of ZCL specification. */
#define BULB_INIT_BASIC_LOCATION_DESC       "Office desk"                       /**< Describes the physical location of the device (16 bytes). May be modified during commisioning process. */
#define BULB_INIT_BASIC_PH_ENV              LIGHT_LOCATION_OFFICE               /**< Describes the type of physical environment. For possible values see section 3.2.2.2.10 of ZCL specification. */
#define BULB_LED_VISIBLE_TRESHOLD           90                                  /**< Threshold for Blink effect. */
#define CHECK_VALUE_CHANGE_PERIOD           120                                 /**< Period of time [ms] to check if value of cluster is changing. */

extern void update_endpoint_led(zb_uint8_t ep, led_params_t * p_led_params);

/* Define nrf app timer to handle too quick cluster attribute value changes in zboss stack */
APP_TIMER_DEF(m_level_timer);
APP_TIMER_DEF(m_effect_timer);
static volatile bool                   m_effect_timer_active;
static zb_color_light_ctx_t * volatile m_p_effect_timer_light_ctx;

/**@brief Function to convert hue_stauration to RGB color space.
 *
 * @param[IN]  hue          Hue value of color.
 * @param[IN]  saturation   Saturation value of color.
 * @param[IN]  brightness   Brightness value of color.
 * @param[OUT] p_led_params Pointer to structure containing parameters to write to LED characteristic
 */
static void convert_hsb_to_rgb(uint8_t hue, uint8_t saturation, uint8_t brightness, led_params_t * p_rgb)
{
    /* Check if p_leds_params is not NULL pointer */
    if (p_rgb == NULL)
    {
        NRF_LOG_INFO("Incorrect pointer to led params");
        return;
    }

    /* C, X, m are auxiliary variables */
    float C     = 0.0;
    float X     = 0.0;
    float m     = 0.0;
    /* Convertion HSB --> RGB */
    C = (brightness / 255.0f) * (saturation / 254.0f);
    X = (hue / 254.0f) * 6.0f;
    /* Casting in var X is necessary due to implementation of floating-point modulo_2 */
    /*lint -e653 */
    X = (X - (2 * (((uint8_t) X) / 2)));
    /*lint -restore */
    X -= 1.0f;
    X = C * (1.0f - ((X > 0.0f) ? (X) : (-1.0f * X)));
    m = (brightness / 255.0f) - C;

    /* Hue value is stored in range (0 - 255) instead of (0 - 360) degree */
    if (hue <= 42) /* hue < 60 degree */
    {
        p_rgb->r = (uint8_t)((C + m) * 255.0f);
        p_rgb->g = (uint8_t)((X + m) * 255.0f);
        p_rgb->b = (uint8_t)((0.0f + m) * 255.0f);
    }
    else if (hue <= 84)  /* hue < 120 degree */
    {
        p_rgb->r = (uint8_t)((X + m) * 255.0f);
        p_rgb->g = (uint8_t)((C + m) * 255.0f);
        p_rgb->b = (uint8_t)((0.0f + m) * 255.0f);
    }
    else if (hue <= 127) /* hue < 180 degree */
    {
        p_rgb->r = (uint8_t)((0.0f + m) * 255.0f);
        p_rgb->g = (uint8_t)((C + m) * 255.0f);
        p_rgb->b = (uint8_t)((X + m) * 255.0f);
    }
    else if (hue < 170)  /* hue < 240 degree */
    {
        p_rgb->r = (uint8_t)((0.0f + m) * 255.0f);
        p_rgb->g = (uint8_t)((X + m) * 255.0f);
        p_rgb->b = (uint8_t)((C + m) * 255.0f);
    }
    else if (hue <= 212) /* hue < 300 degree */
    {
        p_rgb->r = (uint8_t)((X + m) * 255.0f);
        p_rgb->g = (uint8_t)((0.0f + m) * 255.0f);
        p_rgb->b = (uint8_t)((C + m) * 255.0f);
    }
    else                /* hue < 360 degree */
    {
        p_rgb->r = (uint8_t)((C + m) * 255.0f);
        p_rgb->g = (uint8_t)((0.0f + m) * 255.0f);
        p_rgb->b = (uint8_t)((X + m) * 255.0f);
    }
}

/**@brief Function for updating RGB color value.
 *
 * @param[IN] p_ep_dev_ctx pointer to endpoint device ctx.
 */
static void led_update_state(zb_color_light_ctx_t * p_light_ctx)
{
    convert_hsb_to_rgb(p_light_ctx->color_control_attr.set_color_info.current_hue,
                       p_light_ctx->color_control_attr.set_color_info.current_saturation,
                       p_light_ctx->level_control_attr.current_level,
                       &p_light_ctx->led_params);

    update_endpoint_led(p_light_ctx->ep_id, &p_light_ctx->led_params);
}

/**@brief Switch LED off.
 *
 * @param[IN] p_ep_dev_ctx pointer to endpoint device ctx.
 */
static void led_off(zb_color_light_ctx_t * p_light_ctx)
{
    p_light_ctx->led_params.r = 0;
    p_light_ctx->led_params.g = 0;
    p_light_ctx->led_params.b = 0;
    update_endpoint_led(p_light_ctx->ep_id, &p_light_ctx->led_params);
}

/**@brief Function for changing the hue of the light bulb.
 *
 * @param[IN] p_ep_dev_ctx  Pointer to endpoint device ctx.
 * @param[IN] new_hue       New value for hue.
 */
static void light_set_hue(zb_color_light_ctx_t * p_light_ctx, zb_uint8_t hue)
{
    NRF_LOG_INFO("Set color hue value: %i on endpoint: %hu", hue, p_light_ctx->ep_id);

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id,                                       
                         ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,            
                         ZB_ZCL_CLUSTER_SERVER_ROLE,                 
                         ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, 
                         &hue,
                         ZB_FALSE);                                  

    led_update_state(p_light_ctx);
}

/**@brief Function for changing the saturation of the light bulb.
 *
 * @param[IN] p_ep_dev_ctx   pointer to endpoint device ctx.
 * @param[IN] new_saturation new value for saturation.
 */
static void light_set_saturation(zb_color_light_ctx_t * p_light_ctx, zb_uint8_t saturation)
{
    NRF_LOG_INFO("Set color saturation value: %i on endpoint: %hu", saturation, p_light_ctx->ep_id);

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id,                                       
                         ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,            
                         ZB_ZCL_CLUSTER_SERVER_ROLE,                 
                         ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, 
                         &saturation,                                       
                         ZB_FALSE);                                  

    led_update_state(p_light_ctx);
}

/**@brief Function for setting the light bulb brightness.
 *
 * @param[IN] p_ep_dev_ctx Pointer to endpoint device ctx.
 * @param[IN] new_level    Light bulb brightness value.
 */
static void light_set_brightness(zb_color_light_ctx_t * p_light_ctx, zb_uint16_t level)
{
    NRF_LOG_INFO("Set level value: %i on endpoint: %hu", level, p_light_ctx->ep_id);

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id,                                       
                         ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,            
                         ZB_ZCL_CLUSTER_SERVER_ROLE,                 
                         ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, 
                         (zb_uint8_t *)&level,                                       
                         ZB_FALSE);                                  

    if (level == 0)
    {
        zb_uint8_t value = ZB_FALSE;
        ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id, 
                             ZB_ZCL_CLUSTER_ID_ON_OFF,    
                             ZB_ZCL_CLUSTER_SERVER_ROLE,  
                             ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                             &value,                        
                             ZB_FALSE);                   
    }
    else
    {
        zb_uint8_t value = ZB_TRUE;
        ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id, 
                             ZB_ZCL_CLUSTER_ID_ON_OFF,    
                             ZB_ZCL_CLUSTER_SERVER_ROLE,  
                             ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                             &value,                        
                             ZB_FALSE);
    }

    led_update_state(p_light_ctx);
}

/**@brief Function for turning ON/OFF the light bulb.
 *
 * @param[IN] p_ep_dev_ctx Pointer to endpoint device ctx.
 * @param[IN] on           Boolean light bulb state.
 */
static void light_set_state(zb_color_light_ctx_t * p_light_ctx, zb_bool_t on)
{
    NRF_LOG_INFO("Set ON/OFF value: %i on endpoint: %hu", on, p_light_ctx->ep_id);

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id, 
                         ZB_ZCL_CLUSTER_ID_ON_OFF,    
                         ZB_ZCL_CLUSTER_SERVER_ROLE,  
                         ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                         (zb_uint8_t *)&on,                        
                         ZB_FALSE);                   

    if (on)
    {
        light_set_brightness(p_light_ctx,
                             p_light_ctx->level_control_attr.current_level);
    }
    else
    {
        led_off(p_light_ctx);
    }
}

/**@brief Set level control value if stable.
 *
 * This function checks if level control attribute is stable. If so then
 * the LED value is updated. If not another check is scheduled.
 *
 * @param[IN]   context   Void pointer to context function is called with.
 *
 * @details Function is called with pointer to zb_color_light_ctx_t as argument.
 */
static void level_timer_handler(void * context)
{
    zb_color_light_ctx_t  * p_device         = (zb_color_light_ctx_t *)context;
    zb_uint8_t            * p_lvl_ctrl_value = &p_device->level_control_attr.current_level;

    if (p_device->prev_lvl_ctrl_value == *p_lvl_ctrl_value)
    {
        p_device->value_unstable = ZB_FALSE;
        light_set_brightness(p_device, *p_lvl_ctrl_value);
    }
    else
    {
        ret_code_t err_code;
        err_code = app_timer_start(m_level_timer,
                                   APP_TIMER_TICKS(CHECK_VALUE_CHANGE_PERIOD),
                                   context);
        APP_ERROR_CHECK(err_code);

        p_device->prev_lvl_ctrl_value = *p_lvl_ctrl_value;
    }
}

/**@brief Function for initializing clusters attributes.
 *
 * @param[IN]   p_light_ctx   Pointer to structure with device_ctx.
 * @param[IN]   ep_id          Endpoint ID.
 */
static void clusters_attr_init(zb_color_light_ctx_t * p_light_ctx)
{
    /* Basic cluster attributes data */
    p_light_ctx->basic_attr.zcl_version   = ZB_ZCL_VERSION;
    p_light_ctx->basic_attr.app_version   = BULB_INIT_BASIC_APP_VERSION;
    p_light_ctx->basic_attr.stack_version = BULB_INIT_BASIC_STACK_VERSION;
    p_light_ctx->basic_attr.hw_version    = BULB_INIT_BASIC_HW_VERSION;

    /* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte should
     * contain string length without trailing zero.
     *
     * For example "test" string wil be encoded as:
     *   [(0x4), 't', 'e', 's', 't']
     */
    ZB_ZCL_SET_STRING_VAL(p_light_ctx->basic_attr.mf_name,
                          BULB_INIT_BASIC_MANUF_NAME,
                          ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MANUF_NAME));

    ZB_ZCL_SET_STRING_VAL(p_light_ctx->basic_attr.model_id,
                          BULB_INIT_BASIC_MODEL_ID,
                          ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MODEL_ID));

    ZB_ZCL_SET_STRING_VAL(p_light_ctx->basic_attr.date_code,
                          BULB_INIT_BASIC_DATE_CODE,
                          ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_DATE_CODE));

    p_light_ctx->basic_attr.power_source = BULB_INIT_BASIC_POWER_SOURCE;

    ZB_ZCL_SET_STRING_VAL(p_light_ctx->basic_attr.location_id,
                          BULB_INIT_BASIC_LOCATION_DESC,
                          ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_LOCATION_DESC));


    p_light_ctx->basic_attr.ph_env = BULB_INIT_BASIC_PH_ENV;

    /* Identify cluster attributes data */
    p_light_ctx->identify_attr.identify_time       = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

    /* On/Off cluster attributes data */
    p_light_ctx->on_off_attr.on_off                = (zb_bool_t)ZB_ZCL_ON_OFF_IS_ON;
    p_light_ctx->on_off_attr.global_scene_ctrl     = ZB_TRUE;
    p_light_ctx->on_off_attr.on_time               = 0;
    p_light_ctx->on_off_attr.off_wait_time         = 0;

    /* Level control cluster attributes data */
    p_light_ctx->level_control_attr.current_level  = ZB_ZCL_LEVEL_CONTROL_LEVEL_MAX_VALUE; // Set current level value to maximum
    p_light_ctx->level_control_attr.remaining_time = ZB_ZCL_LEVEL_CONTROL_REMAINING_TIME_DEFAULT_VALUE;

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id, 
                         ZB_ZCL_CLUSTER_ID_ON_OFF,    
                         ZB_ZCL_CLUSTER_SERVER_ROLE,  
                         ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                         (zb_uint8_t *)&p_light_ctx->on_off_attr.on_off,                        
                         ZB_FALSE);                   

    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id,                                       
                         ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,            
                         ZB_ZCL_CLUSTER_SERVER_ROLE,                 
                         ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, 
                         (zb_uint8_t *)&p_light_ctx->level_control_attr.current_level,                                       
                         ZB_FALSE);                                  

    /* Color control cluster attributes data */
    zb_zcl_color_ctrl_attrs_set_color_inf_t * p_color_info = &p_light_ctx->color_control_attr.set_color_info;
    p_color_info->current_hue         = ZB_ZCL_COLOR_CONTROL_HUE_RED;
    p_color_info->current_saturation  = ZB_ZCL_COLOR_CONTROL_CURRENT_SATURATION_MAX_VALUE;
    /* Set to use hue & saturation */
    p_color_info->color_mode          = ZB_ZCL_COLOR_CONTROL_COLOR_MODE_HUE_SATURATION;
    p_color_info->color_temperature   = ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
    p_color_info->remaining_time      = ZB_ZCL_COLOR_CONTROL_REMAINING_TIME_MIN_VALUE;
    p_color_info->color_capabilities  = ZB_ZCL_COLOR_CONTROL_CAPABILITIES_HUE_SATURATION;
    /* According to ZCL spec 5.2.2.2.1.12 0x00 shall be set when CurrentHue and CurrentSaturation are used. */
    p_color_info->enhanced_color_mode = 0x00;
    /* According to 5.2.2.2.1.10 execute commands when device is off. */
    p_color_info->color_capabilities  = ZB_ZCL_COLOR_CONTROL_OPTIONS_EXECUTE_IF_OFF;
    /* According to ZCL spec 5.2.2.2.2 0xFF shall be set when specific value is unknown. */
    p_light_ctx->color_control_attr.set_defined_primaries_info.number_primaries = 0xff;
}

/**@brief Clears bool variable in ISR-safe manner (atomically), returns value as it was just before the operation.
 * @param p_variable    Pointer to variable to be read and cleared atomically
 * @return Value of the variable just before the operation
 */
static bool prv_atomic_bool_test_and_clear(volatile bool * p_variable)
{
    bool    result;

    CRITICAL_REGION_ENTER();

    result      = *p_variable;
    *p_variable = false;

    CRITICAL_REGION_EXIT();

    return result;
}

static void effect_timer_handler(void * context)
{
    if (!prv_atomic_bool_test_and_clear(&m_effect_timer_active))
    {
        return;
    }

    zb_color_light_ctx_t * p_light_ctx = (zb_color_light_ctx_t *)context;
    zb_ret_t               ret;

    ret = zb_color_light_do_identify_effect(p_light_ctx,
                                            ZB_ZCL_IDENTIFY_EFFECT_ID_STOP);
    UNUSED_RETURN_VALUE(ret);
}


zb_ret_t zb_color_light_do_identify_effect(zb_color_light_ctx_t * p_light_ctx,
                                           zb_uint8_t             effect_id)
{
    uint8_t      effect_time = 0; // in seconds, 0 for indefinite effect
    led_params_t led_params;
    ret_code_t   err_code = NRF_SUCCESS;

    NRF_LOG_INFO("Identify effect %d on ep %d", effect_id, p_light_ctx->ep_id);

    switch (effect_id)
    {
        case ZB_ZCL_IDENTIFY_EFFECT_ID_BLINK:
            if (p_light_ctx->led_params.mode == LED_MODE_CONSTANT)
            {
                // Current brightness may be set to a low level (below
                // BULB_LED_VISIBLE_TRESHOLD), so switching it off would not
                // produce a blink effect. If, so then switch on fully.
                if (p_light_ctx->led_params.r +
                    p_light_ctx->led_params.g +
                    p_light_ctx->led_params.b > BULB_LED_VISIBLE_TRESHOLD)
                {
                    led_params.r = 0x00;
                    led_params.g = 0x00;
                    led_params.b = 0x00;
                }
                else
                {
                    led_params.r = 0xFF;
                    led_params.g = 0xFF;
                    led_params.b = 0xFF;
                }
            }
            else
            {
                if (p_light_ctx->led_params.color > 0)
                {
                    led_params.r = 0x00;
                    led_params.g = 0x00;
                    led_params.b = 0x00;
                }
                else
                {
                    led_params.r = 0xFF;
                    led_params.g = 0xFF;
                    led_params.b = 0xFF;
                }
            }
            led_params.mode = LED_MODE_CONSTANT;
            effect_time = 1;
            break;

        case ZB_ZCL_IDENTIFY_EFFECT_ID_BREATHE:
            led_params.mode      = LED_MODE_BREATHING;
            led_params.color     = 0x02;
            led_params.intensity = 100;
            led_params.delay     = 50;
            break;

        case ZB_ZCL_IDENTIFY_EFFECT_ID_OKAY:
            led_params.mode    = LED_MODE_CONSTANT;
            led_params.r       = 0x00;
            led_params.g       = 0xFF;
            led_params.b       = 0x00;
            effect_time        = 1;
            break;

        case ZB_ZCL_IDENTIFY_EFFECT_ID_CHANNEL_CHANGE:
            led_params.mode    = LED_MODE_CONSTANT;
            led_params.r       = 0xFF;
            led_params.g       = 0xA5;
            led_params.b       = 0x00;
            effect_time        = 8;
            break;

        case ZB_ZCL_IDENTIFY_EFFECT_ID_FINISH_EFFECT:
        case ZB_ZCL_IDENTIFY_EFFECT_ID_STOP:
            update_endpoint_led(p_light_ctx->ep_id,
                                &p_light_ctx->led_params);
            return RET_OK;

        default:
            return RET_INVALID_PARAMETER;
    }

    if (prv_atomic_bool_test_and_clear(&m_effect_timer_active))
    {
        /* Here even if m_effect_timer's callback (effect_timer_handler) is called, nothing will happen,
         * see effect_timer_handler guard with m_effect_timer_active.
         * We will never get here when called from effect_timer_handler due to the m_effect_timer_active guard
         */
        UNUSED_RETURN_VALUE(app_timer_stop(m_effect_timer));

        zb_color_light_ctx_t * p_effect_timer_light_ctx = m_p_effect_timer_light_ctx;
        if (p_effect_timer_light_ctx != NULL)
        {
            /* Restore led state */
            update_endpoint_led(p_effect_timer_light_ctx->ep_id,
                                &p_effect_timer_light_ctx->led_params);
        }
    }

    update_endpoint_led(p_light_ctx->ep_id, &led_params);

    if (effect_time > 0)
    {
        m_p_effect_timer_light_ctx = p_light_ctx;
        m_effect_timer_active      = true;

        err_code = app_timer_start(m_effect_timer,
                                   APP_TIMER_TICKS(1000*effect_time),
                                   p_light_ctx);

        if (err_code != NRF_SUCCESS)
        {
            m_effect_timer_active = false;
        }
    }


    return (err_code == NRF_SUCCESS ? RET_OK : RET_ERROR);
}

zb_ret_t zb_color_light_set_attribute(zb_color_light_ctx_t          * p_light_ctx,
                                      zb_zcl_set_attr_value_param_t * p_savp)
{
    zb_ret_t ret = RET_NOT_IMPLEMENTED;

    if (p_savp->cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF)
    {
        if (p_savp->attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
        {
            uint8_t value = p_savp->values.data8;
            light_set_state(p_light_ctx, (zb_bool_t)value);
            ret = RET_OK;
            NRF_LOG_INFO("on/off attribute setting to %hd", value);
        }
    }
    else if (p_savp->cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL)
    {
        if (p_savp->attr_id == ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID)
        {
            uint16_t value = p_savp->values.data16;
            light_set_brightness(p_light_ctx, value);
            ret = RET_OK;
            NRF_LOG_INFO("level control attribute setting to %hd", value);
        }
    }
    else if (p_savp->cluster_id == ZB_ZCL_CLUSTER_ID_COLOR_CONTROL)
    {
        if (p_light_ctx->color_control_attr.set_color_info.remaining_time <= 1)
        {
            uint16_t value = p_savp->values.data16;

            switch (p_savp->attr_id)
            {
                case ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID:
                    light_set_hue(p_light_ctx, value);
                    ret = RET_OK;
                    break;

                case ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID:
                    light_set_saturation(p_light_ctx, value);
                    ret = RET_OK;
                    break;

                default:
                    NRF_LOG_INFO("Unused attribute");
                    break;
            }
        }
    }
    else
    {
        /* Other clusters can be processed here */
        NRF_LOG_INFO("Unhandled cluster attribute id: %d", p_savp->cluster_id);
    }

    return ret;
}

zb_ret_t zb_color_light_set_level(zb_color_light_ctx_t * p_light_ctx, zb_uint8_t value)
{
    ret_code_t err_code = NRF_SUCCESS;

    NRF_LOG_INFO("Level control setting to %d", value);
    ZB_ZCL_SET_ATTRIBUTE(p_light_ctx->ep_id,                                       
                         ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,            
                         ZB_ZCL_CLUSTER_SERVER_ROLE,                 
                         ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, 
                         (zb_uint8_t *)&value,                                       
                         ZB_FALSE);                                  

    if (p_light_ctx->value_debounce_time > 0)
    {
        if (p_light_ctx->value_unstable == ZB_FALSE)
        {
            err_code = app_timer_start(m_level_timer,
                                       APP_TIMER_TICKS(p_light_ctx->value_debounce_time),
                                       p_light_ctx);
            p_light_ctx->prev_lvl_ctrl_value = p_light_ctx->level_control_attr.current_level;
            p_light_ctx->value_unstable = ZB_TRUE;
        }
    }
    else
    {
        light_set_brightness(p_light_ctx, value);
    }

    return (err_code == NRF_SUCCESS ? RET_OK : RET_ERROR);
}

void zb_color_light_init_ctx(zb_color_light_ctx_t * p_light_ctx,
                             uint8_t                ep_id,
                             zb_callback_t          identify_cb)
{
    memset(p_light_ctx, 0, sizeof(zb_color_light_ctx_t));

    p_light_ctx->ep_id               = ep_id;
    p_light_ctx->value_unstable      = ZB_FALSE;
    p_light_ctx->value_debounce_time = CHECK_VALUE_CHANGE_PERIOD;
    p_light_ctx->led_params.mode     = LED_MODE_CONSTANT;

    clusters_attr_init(p_light_ctx);
    light_set_brightness(p_light_ctx, ZB_ZCL_LEVEL_CONTROL_LEVEL_MAX_VALUE);

    /* Register handlers to identify notifications */
    ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(p_light_ctx->ep_id, identify_cb);
}

void zb_color_light_init(void)
{
    // Create app timer for handling fast changing cluster attribute values
    uint32_t err_code;

    err_code = app_timer_create(&m_level_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                level_timer_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_effect_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                effect_timer_handler);
    APP_ERROR_CHECK(err_code);
}

/**
 * @}
 */
