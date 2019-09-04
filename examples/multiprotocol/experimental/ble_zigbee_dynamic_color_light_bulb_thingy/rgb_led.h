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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy rgb_led.h
 * @{
 * @ingroup zigbee_examples
 */

#ifndef RGB_LED_H__
#define RGB_LED_H__

#include <stdint.h>

#include "app_util_platform.h"

#ifdef __CC_ARM
#pragma anon_unions
#endif

/* LED modes */
typedef enum led_mode_e
{
    LED_MODE_OFF       = 0,
    LED_MODE_CONSTANT  = 1,
    LED_MODE_BREATHING = 2,
    LED_MODE_ONE_SHOT  = 3
} led_mode_t;

#define LED_PARAMS_COLOR_MASK_RED       0x01U
#define LED_PARAMS_COLOR_MASK_GREEN     0x02U
#define LED_PARAMS_COLOR_MASK_BLUE      0x04U

/** @brief Structure for storing LED configuration */
typedef PACKED_STRUCT led_params_s
{
    /**@brief   Mode of LED behavior.
     * When this field is set to @ref LED_MODE_OFF other fields are ignored.
     * When this field is set to @ref LED_MODE_CONSTANT, fields @c r, @c g, @c b contain required RGB color.
     * When this field is set to @ref LED_MODE_BREATHING, fields @c color, @c intensity, @c delay specify breathing effect appearance.
     * When this field is set to @ref LED_MODE_ONE_SHOT, behavior and required fields are identical to those used with @c mode set to
     * @ref LED_MODE_BREATHING, but only one cycle of breathing effect will be executed, and then the led will switch to mode
     * @ref LED_MODE_OFF automatically.
     */
    led_mode_t mode;

    __PACKED union
    {
        PACKED_STRUCT
        {
            uint8_t  r;         /**< Red color value. */
            uint8_t  g;         /**< Green color value. */
            uint8_t  b;         /**< Blue color value. */
        };
        PACKED_STRUCT
        {
            /**@brief Specifies RGB channels used in breathing mode.
             *
             * Bitwise combination of masks @ref LED_PARAMS_COLOR_MASK_RED, @ref LED_PARAMS_COLOR_MASK_GREEN
             * @ref LED_PARAMS_COLOR_MASK_BLUE.
             */
            uint8_t  color;

            /**@brief Intensity of LED in breathing mode.
             * This field specifies intensity of the LED during most intense lightning phase of the breathing effect.
             * Valid values are from range [0, 100] (intensity in percents).
             */
            uint8_t  intensity;

            /**@brief Amount of time determining how fast LED is breathing (50 ms - 10s).
             * Value of the time is in milliseconds and specifies time between breathe blinks. */
            uint16_t delay;
        };
    };
} led_params_t;

/**@brief Function for initialization of the LED module.
 * @note Must be called before any other function call from this module. @ref app_timer_init must have been
 * successfully called before.
 */
void rgb_led_init(void);

/**@brief Function for updating LED state/behavior.
 * This function just sets requested led state/behavior. Update of visible led state is performed internally and may be delayed.
 *
 * @param[in] p_led_params  A pointer to LED parameters. Must not be NULL.
 */
void rgb_led_update(const led_params_t * p_led_params);

#endif

/**
 * @}
 */
