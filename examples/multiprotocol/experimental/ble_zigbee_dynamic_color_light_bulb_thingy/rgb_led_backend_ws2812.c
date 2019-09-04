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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy rgb_led_backend_ws2812.c
 * @{
 * @ingroup zigbee_examples
 */
#include "sdk_config.h"
#include "rgb_led_backend.h"
#include "app_util_platform.h"
#include "boards.h"
#include "drv_ws2812.h"

/**@def LED_CHAIN_DOUT_PIN
 * @brief GPIO pin used as DOUT (to be connected to DIN pin of the first ws2812 led in chain) */
#ifndef LED_CHAIN_DOUT_PIN
#if defined(BOARD_PCA10056)
#define LED_CHAIN_DOUT_PIN              NRF_GPIO_PIN_MAP(1,7)
#elif defined(BOARD_PCA10059)
#define LED_CHAIN_DOUT_PIN              NRF_GPIO_PIN_MAP(0,29)
#else
#error Unsupported board type
#endif
#endif

static uint32_t m_current_color;

void rgb_led_backend_set_color(uint32_t color)
{
    if (color != m_current_color)
    {
        drv_ws2812_set_pixel_all(color);
        if (drv_ws2812_display(NULL, NULL) == NRF_SUCCESS)
        {
            m_current_color = color;
        }
        else
        {
            /* If previous drv_ws2812_display has not finished yet (very long LED chain and low value of RGB_LED_REFRESH_PERIOD_MS),
             * we simply loose one refresh frame, but without negative effect to overall light effect, because
             * update will happen on next timer tick.
             */
        }
    }
    else
    {
        /* No change in color, just refresh led chain to make device robust to hot plug of led chain */
        UNUSED_RETURN_VALUE(drv_ws2812_refresh(NULL, NULL));
    }
}

void rgb_led_backend_init(void)
{
    ret_code_t ret_code;
    ret_code = drv_ws2812_init(LED_CHAIN_DOUT_PIN);
    APP_ERROR_CHECK(ret_code);

    drv_ws2812_set_pixel_all(0x00000000U);
    UNUSED_RETURN_VALUE(drv_ws2812_display(NULL, NULL));

    m_current_color = 0U;
}

/**
 * @}
 */
