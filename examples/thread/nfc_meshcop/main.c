/**
 * Copyright (c) 2017 - 2019, Nordic Semiconductor ASA
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
 * @defgroup nfc_meshcop_example_main main.c
 * @{
 * @ingroup nfc_meshcop_example
 * @brief An example presenting OpenThread MeshCoP.
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_scheduler.h"
#include "app_timer.h"
#include "bsp_thread.h"
#include "nfc_meshcop_msg.h"
#include "nfc_t2t_lib.h"
#include "nrf_assert.h"
#include "nrf_log_ctrl.h"
#include "nrf_log.h"
#include "nrf_log_default_backends.h"

#include "thread_utils.h"

#include <openthread/cli.h>
#include <openthread/joiner.h>
#include <openthread/thread.h>

#define JOINER_DELAY             500
#define MAX_JOINER_RETRY_COUNT   3

#define APP_TIMER_PRESCALER      0                               // Value of the RTC1 PRESCALER register.
#define APP_TIMER_OP_QUEUE_SIZE  2                               // Size of timer operation queues.

#define EMPTY_VALUE              ""

#define SCHED_QUEUE_SIZE         32                              /**< Maximum number of events in the scheduler queue. */
#define SCHED_EVENT_DATA_SIZE    APP_TIMER_SCHED_EVENT_DATA_SIZE /**< Maximum app_scheduler event size. */

APP_TIMER_DEF(m_joiner_timer);

static uint8_t m_ndef_msg_buf[256];

typedef struct
{
    nfc_meshcop_data_t meshcop_data;                    /**< Commissioning data structure. */
    uint32_t           joiner_retry_count;              /**< Retry counter for commissioning process. */
    bool               commissioning_in_progress;       /**< Indicates that the commissioning is ongoing. */
} application_t;

static application_t m_app =
{
    .meshcop_data              =
    {
        .psk_d = "0A1B2C3D"
    },
    .joiner_retry_count        = 0,
    .commissioning_in_progress = false,
};

/***************************************************************************************************
 * @ Callback functions
 **************************************************************************************************/

static void joiner_callback(otError error, void * p_context)
{
	uint32_t err_code;

    if (error == OT_ERROR_NONE)
    {
        err_code = nfc_t2t_emulation_stop();
        APP_ERROR_CHECK(err_code);

        err_code = bsp_thread_commissioning_indication_set(BSP_INDICATE_COMMISSIONING_SUCCESS);
        APP_ERROR_CHECK(err_code);

        error = otThreadSetEnabled(thread_ot_instance_get(), true);
        ASSERT(error == OT_ERROR_NONE);
    }
    else
    {
        if (m_app.joiner_retry_count < MAX_JOINER_RETRY_COUNT)
        {
            m_app.joiner_retry_count++;

            err_code = app_timer_start(m_joiner_timer, APP_TIMER_TICKS(JOINER_DELAY), thread_ot_instance_get());
            APP_ERROR_CHECK(err_code);
        }
        else
        {
            m_app.joiner_retry_count = 0;
            m_app.commissioning_in_progress = false;

            err_code = bsp_thread_commissioning_indication_set(
                BSP_INDICATE_COMMISSIONING_NOT_COMMISSIONED);
            APP_ERROR_CHECK(err_code);
        }
    }
}

static void nfc_callback(void          * p_context,
                         nfc_t2t_event_t event,
                         const uint8_t * p_data,
                         size_t          data_length)
{
    UNUSED_VARIABLE(p_context);

    switch (event)
    {
        case NFC_T2T_EVENT_FIELD_ON:
            bsp_board_led_on(BSP_BOARD_LED_3);

            if (!m_app.commissioning_in_progress)
            {
                m_app.commissioning_in_progress = true;

                uint32_t err_code = bsp_thread_commissioning_indication_set(
                    BSP_INDICATE_COMMISSIONING_IN_PROGRESS);
                APP_ERROR_CHECK(err_code);

                err_code = app_timer_start(m_joiner_timer, APP_TIMER_TICKS(JOINER_DELAY), thread_ot_instance_get());
                APP_ERROR_CHECK(err_code);
            }
            break;

        case NFC_T2T_EVENT_FIELD_OFF:
            bsp_board_led_off(BSP_BOARD_LED_3);
            break;

        default:
            break;
    }
}

static void bsp_event_handler(bsp_event_t event)
{
    switch (event)
    {
        case BSP_EVENT_KEY_3:
            otInstanceFactoryReset(thread_ot_instance_get());
            break;

        default:
            return;
    }
}

/***************************************************************************************************
 * @section State
 **************************************************************************************************/

static void thread_state_changed_callback(uint32_t flags, void * p_context)
{
    NRF_LOG_INFO("State changed! Flags: 0x%08x Current role: %d\r\n",
                 flags, otThreadGetDeviceRole(p_context));
}

/***************************************************************************************************
 * @section Timers
 **************************************************************************************************/

static void joiner_timer_handler(void * p_context)
{
    otError error = otJoinerStart(p_context,
                                  m_app.meshcop_data.psk_d,
                                  EMPTY_VALUE,
                                  "NordicSemiconductor",
                                  EMPTY_VALUE,
                                  EMPTY_VALUE,
                                  EMPTY_VALUE,
                                  joiner_callback,
                                  p_context);
    ASSERT(error == OT_ERROR_NONE);
}

/***************************************************************************************************
 * @section Initialization

 **************************************************************************************************/

 /**@brief Function for initializing the Application Timer Module
 */
static void timer_init(void)
{
    uint32_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_joiner_timer, APP_TIMER_MODE_SINGLE_SHOT, joiner_timer_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the LEDs.
 */
static void leds_init(void)
{
    LEDS_CONFIGURE(LEDS_MASK);
    LEDS_OFF(LEDS_MASK);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing the Thread Board Support Package
 */
static void thread_bsp_init(otInstance * p_ot_instance)
{
    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_thread_init(p_ot_instance);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Thread Stack
 */
static void thread_instance_init(void)
{
    thread_configuration_t thread_configuration =
    {
        .radio_mode        = THREAD_RADIO_MODE_RX_ON_WHEN_IDLE,
        .autocommissioning = false,
    };

    thread_init(&thread_configuration);
    thread_cli_init();
    thread_state_changed_callback_set(thread_state_changed_callback);
}


/**@brief Function for initializing the Near Field Communication module.
 */
static void nfc_init(void)
{
    // Set up NFC.
    uint32_t err_code = nfc_t2t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    // Provide information about available buffer size to encoding function.
    uint32_t len = sizeof(m_ndef_msg_buf);

    otLinkGetFactoryAssignedIeeeEui64(thread_ot_instance_get(), &m_app.meshcop_data.eui64);

    err_code = nfc_meshcop_msg_encode(&m_app.meshcop_data, m_ndef_msg_buf, &len);
    APP_ERROR_CHECK(err_code);

    // Set created message as the NFC payload.
    err_code = nfc_t2t_payload_set(m_ndef_msg_buf, len);
    APP_ERROR_CHECK(err_code);

    // Start sensing NFC field.
    err_code = nfc_t2t_emulation_start();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/***************************************************************************************************
 * @section Main
 **************************************************************************************************/

int main (int argc, char *argv[])
{
	uint32_t err_code;

    log_init();
    scheduler_init();
    timer_init();
    leds_init();

    thread_instance_init();
    thread_bsp_init(thread_ot_instance_get());

    if (!otDatasetIsCommissioned(thread_ot_instance_get()))
    {
        err_code = bsp_thread_commissioning_indication_set(
            BSP_INDICATE_COMMISSIONING_NOT_COMMISSIONED);
        APP_ERROR_CHECK(err_code);

        nfc_init();
    }
    else
    {
        err_code = bsp_thread_commissioning_indication_set(BSP_INDICATE_COMMISSIONING_SUCCESS);
        APP_ERROR_CHECK(err_code);
    }

    while (true)
    {
        thread_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            thread_sleep();
        }
    }
}

/**
 *@}
 **/
