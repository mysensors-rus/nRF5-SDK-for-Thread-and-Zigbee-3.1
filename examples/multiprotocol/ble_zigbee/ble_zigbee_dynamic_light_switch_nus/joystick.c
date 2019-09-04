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
 * @defgroup zigbee_examples_multiprotocol_nus_switch joystick.c
 * @{
 * @ingroup  zigbee_examples
 * @brief    UART over BLE application with Zigbee HA light switch profile.
 *
 * This file contains the source code for a handling the Arduino Joystick shield in case
 * such variant is used.
 * This application uses the @ref srvlib_conn_params module.
 */
#include "joystick.h"

APP_TIMER_DEF(m_joystick_timer_id);                                                 /**< APP timer that is responsible starting ADC sampling. */

static nrf_saadc_value_t m_joystick_values[2][JOYSTICK_CHANNELS];                   /**< Buffer used during ADC conversion, stores current joystick state. */
static joystick_cb_t m_cb;

/**@biref SAADC driver callback function, informing about finished ADC conversion.
 *
 * @param[in]  p_event  Pointer to the SAADC event structure.
 */
static void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        ret_code_t err_code;

        // Schedule next ADC conversion.
        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 2);
        APP_ERROR_CHECK(err_code);

        // Apply bounds on measured values.
        for (int i = 0; i < p_event->data.done.size; i++)
        {
            if (p_event->data.done.p_buffer[i] < JOYSTICK_MIN_VALUE)
            {
                p_event->data.done.p_buffer[i] = JOYSTICK_MIN_VALUE;
            }
            else if (p_event->data.done.p_buffer[i] > JOYSTICK_MAX_VALUE)
            {
                p_event->data.done.p_buffer[i] = JOYSTICK_MAX_VALUE;
            }

        }

        float x = (p_event->data.done.p_buffer[0] - JOYSTICK_MIN_VALUE) - (JOYSTICK_MAX_VALUE - JOYSTICK_MIN_VALUE) / 2.0F;
        float y = (p_event->data.done.p_buffer[1] - JOYSTICK_MIN_VALUE) - (JOYSTICK_MAX_VALUE - JOYSTICK_MIN_VALUE) / 2.0F;
        NRF_LOG_DEBUG("Joystick position: x = %d y = %d ", (int32_t) x, (int32_t) y);

        float radius = sqrtf(x * x + y * y);
        radius = radius * 512.0F / (JOYSTICK_MAX_VALUE - JOYSTICK_MIN_VALUE);
        if (radius > 254.0F)
        {
            radius = 254.0F;
        }

        uint8_t saturation = (uint8_t) radius;
        NRF_LOG_DEBUG("Joystick deflection: %d ", saturation);

        // Skip if joystick is inside idle position.
        if (saturation < 50)
        {
           return;
        }

        float angle = atanf(x / y);
        if (y < 0.0F)
        {
            angle += (float)M_PI;
        }
        else if (x < 0.0F)
        {
            angle += (float)M_PI * 2.0F;
        }
        NRF_LOG_DEBUG("Joystick angle: %d ", (int32_t) ((angle * 180.0F) / (float)M_PI))

        // Scale anr normalize angle to get hue value.
        uint8_t hue = (uint8_t) (angle * 254.0F / (2.0F * (float)M_PI));

        uint32_t hue_saturation = 0;
        hue_saturation = hue_saturation | hue << 8 | saturation;

        if (m_cb)
        {
            /* Fire up the callback. */
            m_cb((void *) hue_saturation);
        }
    }
}

/**@brief Function for initialising SAADC driver used to read jostick position. */
static void saadc_init(void)
{
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_x_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(JOYSTICK_V_SAADC_INPUT);
    nrf_saadc_channel_config_t channel_y_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(JOYSTICK_H_SAADC_INPUT);

    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_channel_init(0, &channel_x_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_channel_init(1, &channel_y_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_joystick_values[0], JOYSTICK_CHANNELS);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_joystick_values[1], JOYSTICK_CHANNELS);
    APP_ERROR_CHECK(err_code);
}

/**@brief Applicatin timer callback, starting next ADC conversion.
 *
 * @param[in]  p_context  Callback context. Not used.
 */
static void joystick_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    ret_code_t err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
}

void joystick_init(joystick_cb_t cb)
{
    uint32_t error_code = app_timer_create(&m_joystick_timer_id, APP_TIMER_MODE_REPEATED, joystick_meas_timeout_handler);
    APP_ERROR_CHECK(error_code);

    saadc_init();
    error_code = app_timer_start(m_joystick_timer_id, APP_TIMER_TICKS(200), NULL);
    APP_ERROR_CHECK(error_code);

    m_cb = cb;
}
