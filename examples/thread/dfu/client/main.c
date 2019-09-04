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
 * @defgroup thread_secure_dfu_example_main main.c
 * @{
 * @ingroup thread_secure_dfu_example
 * @brief Thread Secure DFU Example Application main file.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "app_scheduler.h"
#include "app_util.h"
#include "app_timer.h"
#include "boards.h"
#include "bsp_thread.h"
#include "nrf_delay.h"
#include "mem_manager.h"
#include "nrf_assert.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "sdk_config.h"
#include "nrf_dfu_utils.h"
#include "coap_dfu.h"
#include "background_dfu_state.h"

#include "thread_utils.h"

#include <openthread/diag.h>
#include <openthread/cli.h>
#include <openthread/thread.h>
#include <openthread/platform/alarm-micro.h>
#include <openthread/platform/alarm-milli.h>

#define SCHED_QUEUE_SIZE                32                                            /**< Maximum number of events in the scheduler queue. */
#define SCHED_EVENT_DATA_SIZE           APP_TIMER_SCHED_EVENT_DATA_SIZE               /**< Maximum app_scheduler event size. */

extern const app_timer_id_t nrf_dfu_inactivity_timeout_timer_id;
void handle_dfu_command(int argc, char *argv[]);

APP_TIMER_DEF(m_coap_tick_timer);    /**< Timer used by this module. */

static otCliCommand m_user_commands[] =
{
    {
        .mName = "dfu",
        .mCommand = handle_dfu_command
    }
};


__WEAK bool nrf_dfu_button_enter_check(void)
{
    // Dummy function for Keil compilation. This should not be called.
    return false;
}


__WEAK void nrf_bootloader_app_start(uint32_t start_addr)
{
    (void)start_addr;
    // Dummy function for Keil compilation. This should not be called.
}


void handle_dfu_command(int argc, char *argv[])
{
    if (argc == 0)
    {
        otCliAppendResult(OT_ERROR_PARSE);
        return;
    }

    if (strcmp(argv[0], "diag") == 0)
    {
        struct background_dfu_diagnostic diag;
        coap_dfu_diagnostic_get(&diag);
        otCliOutputFormat("build_id: 0x%08x, "
                              "state: %d, "
                              "prev_state: %d, ",
                              diag.build_id,
                              diag.state,
                              diag.prev_state);
        otCliOutputFormat("\r\n");
        otCliAppendResult(OT_ERROR_NONE);
    }
}


void coap_dfu_handle_error(void)
{
    coap_dfu_reset_state();
}


static void address_print(const otIp6Address *addr)
{
    char ipstr[40];
    snprintf(ipstr, sizeof(ipstr), "%x:%x:%x:%x:%x:%x:%x:%x",
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 0)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 1)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 2)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 3)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 4)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 5)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 6)),
             uint16_big_decode((uint8_t *)(addr->mFields.m16 + 7)));

    NRF_LOG_INFO("%s\r\n", (uint32_t)ipstr);
}


static void addresses_print(otInstance * aInstance)
{
    for (const otNetifAddress *addr = otIp6GetUnicastAddresses(aInstance); addr; addr = addr->mNext)
    {
        address_print(&addr->mAddress);
    }
}

/**@brief Function for initializing scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

static void state_changed_callback(uint32_t aFlags, void *aContext)
{
    if (aFlags & OT_CHANGED_THREAD_NETDATA)
    {
        addresses_print(thread_ot_instance_get());
    }

    otDeviceRole role = otThreadGetDeviceRole(thread_ot_instance_get());
    NRF_LOG_INFO("New role: %d\r\n", role);

    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        switch(role)
        {
            case OT_DEVICE_ROLE_CHILD:
            case OT_DEVICE_ROLE_ROUTER:
            case OT_DEVICE_ROLE_LEADER:
                coap_dfu_trigger(NULL);
                break;

            case OT_DEVICE_ROLE_DISABLED:
            case OT_DEVICE_ROLE_DETACHED:
            default:
                break;
        }
    }
}

/**@brief Handle events from m_coap_tick_timer.
 */
static void nrf_coap_time_tick_handler(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    coap_time_tick();
}


/**@brief Function for creating coap tick timer.
 */
static ret_code_t coap_tick_timer_create(void)
{
     ret_code_t ret_code = app_timer_create(&m_coap_tick_timer,
                                           APP_TIMER_MODE_REPEATED,
                                           nrf_coap_time_tick_handler);
    if (ret_code != NRF_SUCCESS)
    {
        return ret_code;
    }

    return app_timer_start(m_coap_tick_timer, APP_TIMER_TICKS(1000), NULL);
}


/**@brief Function for initializing the Thread Board Support Package.
 */
static void thread_bsp_init(void)
{
    uint32_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_thread_init(thread_ot_instance_get());
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Thread Stack.
 */
static void thread_instance_init(void)
{
    thread_configuration_t thread_configuration =
    {
        .radio_mode        = THREAD_RADIO_MODE_RX_ON_WHEN_IDLE,
        .autocommissioning = true,
    };

    thread_init(&thread_configuration);
    thread_cli_init();
    thread_state_changed_callback_set(state_changed_callback);

    otCliSetUserCommands(m_user_commands, sizeof(m_user_commands) / sizeof(otCliCommand));
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/***************************************************************************************************
 * @section Main
 **************************************************************************************************/

int main(int argc, char *argv[])
{
    log_init();

    uint32_t err_code = nrf_mem_init();
    APP_ERROR_CHECK(err_code);

    scheduler_init();

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    thread_instance_init();

    err_code = coap_dfu_init(thread_ot_instance_get());
    APP_ERROR_CHECK(err_code);

    thread_bsp_init();

    coap_tick_timer_create();

    while (true)
    {
        coap_dfu_process();

        thread_process();
        app_sched_execute();

        NRF_LOG_PROCESS();
    }
}

/** @} */
