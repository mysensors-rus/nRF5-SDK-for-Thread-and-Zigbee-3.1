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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy rgb_led.c
 * @{
 * @ingroup zigbee_examples
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "app_util_platform.h"
#include "app_timer.h"
#include "rgb_led.h"
#include "rgb_led_backend.h"

/**@def RGB_LED_REFRESH_PERIOD_MS
 * @brief Period of timer performing refresh of RGB led chain
 */
#ifndef RGB_LED_REFRESH_PERIOD_MS
#define RGB_LED_REFRESH_PERIOD_MS   (40U)
#endif

static led_params_t  m_curr_led_params;
static volatile led_params_t  m_next_led_params;
static volatile bool m_next_led_params_set;
static uint32_t m_timer_ms;
static uint32_t m_breathe_delay_start_timestamp;
static bool m_breathe_delay_state;
/* Index to c_led_breathe_brightness_sequence */
static size_t  m_led_breathe_sequence_curr_idx;
/* LED brightness sequence, played to imitate 'breathe' effect. */
static const uint8_t  c_led_breathe_brightness_sequence[] =
{
        0, 10, 20, 40, 80, 120, 160, 200, 240, 255, 240, 200, 160, 120, 80, 40, 20, 10, 0
};

APP_TIMER_DEF(m_led_refresh_timer);

/**@brief Function for applying intensity to given brightness.
 *
 * @param[in] brightness    Value from range [0, 255] being brightness of color (255 means the brightest)
 * @param[in] intensity     Value from range [0, 100] being intensity of color
 *
 * @return Color brightness multiplied by color intensity
 */
static uint8_t brightness_apply_intensity(uint8_t brightness, uint8_t intensity)
{
    if (intensity > 100U)
    {
        /* Limit intensity value, defensive code */
        intensity = 100U;
    }

    return (uint8_t)((uint32_t)intensity * brightness / 100U);
}

/**@brief Function for making RGB color value out of mask selecting individual channels and channel brightness
 * @param[in] brightness    Value from range [0, 255] being brightness of selected channels
 * @param[in] color_mask    Mask selecting RGB channels, combination of @ref LED_PARAMS_COLOR_MASK_RED,
 *                          @ref LED_PARAMS_COLOR_MASK_GREEN, @ref LED_PARAMS_COLOR_MASK_BLUE flags.
 *
 * @return RGB color value suitable for RGB LED backend module.
 */
static uint32_t make_rgb_color_from_brightness_and_mask(uint8_t brightness, uint8_t color_mask)
{
    uint32_t result = 0U;

    /* Here component takes value from range [0, 255] */
    if ((color_mask & LED_PARAMS_COLOR_MASK_RED) != 0U)
    {
        result |= brightness;
    }
    result <<= 8;

    if ((color_mask & LED_PARAMS_COLOR_MASK_GREEN) != 0U)
    {
        result |= brightness;
    }
    result <<= 8;

    if ((color_mask & LED_PARAMS_COLOR_MASK_BLUE) != 0U)
    {
        result |= brightness;
    }

    return result;
}

/**@brief Function for converting led_params_t (r, g, b components) into RGB LED backend color.
 *
 * @param[in] p_led_params  Input color structure with filled @c r, @c g, @c fields
 *
 * @return RGB color compatible with RGB LED backend module.
 */
static uint32_t make_rgb_color_from_led_params_rgb(const led_params_t * p_led_params)
{
    uint32_t result;

    result = p_led_params->r;
    result <<= 8;
    result |= p_led_params->g;
    result <<= 8;
    result |= p_led_params->b;

    return result;
}

/**@brief Function for generating RGB color compatible with RGB LED backend module from led_params_t and given index of breathe sequence
 *
 * @param[in] p_led_params          Input led parameters with filled @c intensity and @c color fields.
 * @param[in] breathe_sequence_idx  Index to @ref c_led_breathe_brightness_sequence
 *
 * @return RGB color compatible with RGB LED backend module corresponding to given step of breathe sequence.
 */
static uint32_t make_rgb_color_from_breathe_sequence(const led_params_t * p_led_params, size_t breathe_sequence_idx)
{
    uint8_t brightness;

    brightness = c_led_breathe_brightness_sequence[breathe_sequence_idx];
    brightness = brightness_apply_intensity(brightness, p_led_params->intensity);

    return make_rgb_color_from_brightness_and_mask(brightness, p_led_params->color);
}

/**@brief Function for generating RGB color compatible with RGB LED backend module form current LED controlling variables
 *
 * @return RGB color compatible with RGB LED backend module.
 */
static uint32_t get_current_state_color(void)
{
    uint32_t color;

    switch (m_curr_led_params.mode)
    {
        case LED_MODE_CONSTANT:
            color = make_rgb_color_from_led_params_rgb(&m_curr_led_params);
            break;

        case LED_MODE_BREATHING:
            /* no break, fall-through */
        case LED_MODE_ONE_SHOT:
            color = make_rgb_color_from_breathe_sequence(&m_curr_led_params, m_led_breathe_sequence_curr_idx);
            break;

        case LED_MODE_OFF:
            /* no break, fall-through */
        default:
            color = 0U;
            break;
    }

    return color;
}

/**@brief Function for transition to next breathe sequence
 * @param[in] idx   Previous index to @ref c_led_breathe_brightness_sequence
 * @return Next value of index to @ref c_led_breathe_brightness_sequence to have nice breathe effect */
static size_t breathe_sequence_idx_next(size_t idx)
{
    idx++;
    if (idx >= ARRAY_SIZE(c_led_breathe_brightness_sequence))
    {
        idx = 0U;
    }
    return idx;
}

static void led_refresh_timer_callback(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    m_timer_ms += RGB_LED_REFRESH_PERIOD_MS;    /* Possible wrap around is okay */

    if (m_next_led_params_set)
    {
        /* We need to load a new requested pattern, set current state as requested */
        m_next_led_params_set = false;
        m_curr_led_params = m_next_led_params;
        m_led_breathe_sequence_curr_idx = 0U;
        m_breathe_delay_state = false;
    }
    else
    {
        /* State transitions */
        switch (m_curr_led_params.mode)
        {
            case LED_MODE_BREATHING:
                if (!m_breathe_delay_state)
                {
                    /* Generating breathe sequence */
                    m_led_breathe_sequence_curr_idx = breathe_sequence_idx_next(m_led_breathe_sequence_curr_idx);
                    if ((m_led_breathe_sequence_curr_idx == 0U) && (m_curr_led_params.delay >= RGB_LED_REFRESH_PERIOD_MS))
                    {
                        /* Just about to start a new breathe sequence, but need to wait a delay given by m_curr_led_params.delay */
                        m_breathe_delay_state = true;
                        m_breathe_delay_start_timestamp = m_timer_ms;
                    }
                }
                else if ( (uint32_t)(m_timer_ms - m_breathe_delay_start_timestamp) >= m_curr_led_params.delay)
                {
                    /* Delay after previous breathe sequence has just finished */
                    m_breathe_delay_state = false;
                    m_led_breathe_sequence_curr_idx = breathe_sequence_idx_next(m_led_breathe_sequence_curr_idx);
                }
                else
                {
                    /* Still in delay between breathes */
                }
                break;

            case LED_MODE_ONE_SHOT:
                m_led_breathe_sequence_curr_idx = breathe_sequence_idx_next(m_led_breathe_sequence_curr_idx);
                if (m_led_breathe_sequence_curr_idx == 0U)
                {
                    /* Breathe sequence has just finished */
                    m_curr_led_params.mode = LED_MODE_OFF;
                }
                break;

            default:
                /* No transitions required */
                break;
        }
    }

    rgb_led_backend_set_color(get_current_state_color());
}

void rgb_led_update(const led_params_t * p_led_params)
{
    uint8_t cr_nested;

    app_util_critical_region_enter(&cr_nested);
    m_next_led_params = *p_led_params;
    m_next_led_params_set = true;
    app_util_critical_region_exit(cr_nested);
}

void rgb_led_init(void)
{
    ret_code_t ret_code;

    rgb_led_backend_init();

    m_next_led_params.mode = LED_MODE_OFF;
    m_curr_led_params = m_next_led_params;
    m_next_led_params_set = false;

    ret_code = app_timer_create(&m_led_refresh_timer, APP_TIMER_MODE_REPEATED, led_refresh_timer_callback);
    APP_ERROR_CHECK(ret_code);

    ret_code = app_timer_start(m_led_refresh_timer, APP_TIMER_TICKS(RGB_LED_REFRESH_PERIOD_MS), NULL);
    APP_ERROR_CHECK(ret_code);
}

/**
 * @}
 */
