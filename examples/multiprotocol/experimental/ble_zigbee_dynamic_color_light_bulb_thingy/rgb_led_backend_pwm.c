/**
 * Copyright (c) 2019, Nordic Semiconductor ASA
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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy rgb_led_backend_pwm.c
 * @{
 * @ingroup zigbee_examples
 */
#include <stdint.h>

#include "sdk_config.h"
#include "rgb_led_backend.h"
#include "nrf_gpio.h"
#include "nrf_drv_pwm.h"


#define RGB_LED_PWM_VALUE_MAX      1024                     /**< PWM counter maximum value. */
#define RGB_LED_PWM_VALUE_MIN      35                       /**< Minimal PWM counter value, which lights up the LED. */

#ifndef RGB_LED_BACKEND_PWM_R_PIN
#define RGB_LED_BACKEND_PWM_R_PIN  NRF_GPIO_PIN_MAP(1,12)   /**< Pin number of red LED of the RGB tape. */
#endif

#ifndef RGB_LED_BACKEND_PWM_G_PIN
#define RGB_LED_BACKEND_PWM_G_PIN  NRF_GPIO_PIN_MAP(1,13)   /**< Pin number of green LED of the RGB tape. */
#endif

#ifndef RGB_LED_BACKEND_PWM_B_PIN
#define RGB_LED_BACKEND_PWM_B_PIN  NRF_GPIO_PIN_MAP(1,11)   /**< Pin number of blue LED of the RGB tape. */
#endif

#ifndef RGB_LED_BACKEND_PWM_W_PIN
#define RGB_LED_BACKEND_PWM_W_PIN  NRF_DRV_PWM_PIN_NOT_USED /**< Pin number of white LED of the RGBW tape (unused). */
#endif


/**@brief Union that allows to access color representation as byte array. */
typedef union
{
    uint32_t   combined;  /**< RGB LED color stored as a concatenated (w|r|g|b) color bytes. */
    uint8_t    color[4];  /**< RGB LED color stored as a separate channels. */
} rgb_bulb_color_t;


/* Declare app PWM instance for controlling LED tape. */
static nrf_drv_pwm_t               m_led_pwm = RGB_LED_BACKEND_PWM_INSTANCE;
static nrf_pwm_values_individual_t m_led_values;

static const nrf_pwm_sequence_t m_led_seq =
{
    .values.p_individual = &m_led_values,
    .length              = NRF_PWM_VALUES_LENGTH(m_led_values),
    .repeats             = 0,
    .end_delay           = 0
};

/* LED correction values, in percent, relative to the current brightness level. */
const uint8_t c_led_color_cal[] = {66, 73, 100, 0};

/* Brightness level to PWM counter matrix. */
const uint16_t c_led_brightness[32] = {
    0, 1, 3, 6, 10, 16, 23, 32, 43, 56, 71, 88, 107,
    129, 154, 181, 210, 242, 277, 315, 356, 400, 447,
    498, 551, 608, 668, 732, 799, 869, 944, 1023};


/**@brief Function for converting single-byte brightness level to PWM counter value.
 *
 * @param[in]  brightness  Brightness level in 0-255 range.
 * @param[in]  correction  Brightness correction value, in percent.
 *
 * @returns  PWM counter value.
 **/
static uint16_t brightness_to_pwm(uint8_t brightness, uint8_t correction)
{
    uint16_t pwm_signal;

    // Scale available brightness level from (0 - 255) range down to (0 - 31) range.
    brightness = brightness / 8;

    pwm_signal = RGB_LED_PWM_VALUE_MIN + (c_led_brightness[brightness] * (uint16_t)correction) / 100;

    if (c_led_brightness[brightness] == 0)
    {
        pwm_signal = 0;
    }
    else if (pwm_signal > RGB_LED_PWM_VALUE_MAX)
    {
        pwm_signal = RGB_LED_PWM_VALUE_MAX;
    }

    return RGB_LED_PWM_VALUE_MAX - pwm_signal;
}

void rgb_led_backend_set_color(uint32_t color)
{
    uint16_t         * p_channels = (uint16_t *)&m_led_values;
    rgb_bulb_color_t   displayed_color;

    displayed_color.combined = color;
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        p_channels[i] = brightness_to_pwm(displayed_color.color[i], c_led_color_cal[i]);
    }
}

void rgb_led_backend_init(void)
{
    uint32_t err_code;

    const nrf_drv_pwm_config_t led_pwm_config =
    {
        .output_pins =
        {
            RGB_LED_BACKEND_PWM_B_PIN, // channel 0
            RGB_LED_BACKEND_PWM_G_PIN, // channel 1
            RGB_LED_BACKEND_PWM_R_PIN, // channel 2
            RGB_LED_BACKEND_PWM_W_PIN, // channel 3
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = RGB_LED_PWM_VALUE_MAX,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO
    };

    /* Initialize PWM in order to control dimmable RGB LED tape. */
    err_code = nrf_drv_pwm_init(&m_led_pwm, &led_pwm_config, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_pwm_simple_playback(&m_led_pwm, &m_led_seq, 1, NRF_DRV_PWM_FLAG_LOOP);
    APP_ERROR_CHECK(err_code);
}
