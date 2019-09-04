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
 * @defgroup freertos_coap_server_example_main main.c
 * @{
 * @ingroup freertos_coap_server_example
 *
 * @brief Thread CoAP server example with FreeRTOS Application main file.
 *
 * This file contains the source code for a sample application using Thread CoAP server and FreeRTOS.
 *
 */
#include "FreeRTOS.h"
#include "nrf_drv_clock.h"
#include "task.h"

#define NRF_LOG_MODULE_NAME APP
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#include "app_timer.h"
#include "bsp_thread.h"
#include "thread_coap_utils.h"
#include "thread_utils.h"

#include <openthread/instance.h>
#include <openthread/thread.h>

#ifdef MBEDTLS_THREADING
#include "freertos_mbedtls_mutex.h"
#endif

#define THREAD_STACK_TASK_STACK_SIZE     (( 1024 * 8 ) / sizeof(StackType_t))   /**< FreeRTOS task stack size is determined in multiples of StackType_t. */
#define LOG_TASK_STACK_SIZE              ( 1024 / sizeof(StackType_t))          /**< FreeRTOS task stack size is determined in multiples of StackType_t. */
#define THREAD_STACK_TASK_PRIORITY       2
#define LOG_TASK_PRIORITY                1
#define LED1_TASK_PRIORITY               1
#define LED2_TASK_PRIORITY               1
#define LED1_BLINK_INTERVAL              427
#define LED2_BLINK_INTERVAL              472

#if NRF_LOG_ENABLED
static TaskHandle_t m_logger_task;  /**< Definition of Logger task. */
#endif

typedef struct
{
    TaskHandle_t thread_stack_task;   /**< Thread stack task handle */
    TaskHandle_t led1_task;           /**< LED1 task handle*/
    TaskHandle_t led2_task;           /**< LED2 task handle*/
} application_t;

application_t m_app =
{
    .thread_stack_task = NULL,
    .led1_task         = NULL,
    .led2_task         = NULL,
};

/***************************************************************************************************
 * @section CoAP
 **************************************************************************************************/

static inline void light_on(void)
{
    vTaskResume(m_app.led1_task);
    vTaskResume(m_app.led2_task);
}


static inline void light_off(void)
{
    vTaskSuspend(m_app.led1_task);
    LEDS_OFF(BSP_LED_2_MASK);

    vTaskSuspend(m_app.led2_task);
    LEDS_OFF(BSP_LED_3_MASK);
}


static inline void light_toggle(void)
{
    if (!thread_coap_utils_light_is_led_blinking())
    {
        light_on();
    }
    else
    {
        light_off();
    }
}


static void on_light_change(thread_coap_utils_light_command_t command)
{
    switch (command)
    {
        case THREAD_COAP_UTILS_LIGHT_CMD_ON:
            light_on();
            break;

        case THREAD_COAP_UTILS_LIGHT_CMD_OFF:
            light_off();
            break;

        case THREAD_COAP_UTILS_LIGHT_CMD_TOGGLE:
            light_toggle();
            break;

        default:
            ASSERT(false);
            break;
    }
}

/***************************************************************************************************
 * @section Signal handling
 **************************************************************************************************/

void otTaskletsSignalPending(otInstance * p_instance)
{
    if (m_app.thread_stack_task == NULL)
    {
        return;
    }

    UNUSED_RETURN_VALUE(xTaskNotifyGive(m_app.thread_stack_task));
}


void otSysEventSignalPending(void)
{
    static BaseType_t xHigherPriorityTaskWoken;

    if (m_app.thread_stack_task == NULL)
    {
        return;
    }

    vTaskNotifyGiveFromISR(m_app.thread_stack_task, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/***************************************************************************************************
 * @section State change handling
 **************************************************************************************************/

 static void thread_state_changed_callback(uint32_t flags, void * p_context)
{
    if (flags & OT_CHANGED_THREAD_ROLE)
    {
        switch(otThreadGetDeviceRole(p_context))
        {
            case OT_DEVICE_ROLE_CHILD:
            case OT_DEVICE_ROLE_ROUTER:
            case OT_DEVICE_ROLE_LEADER:
                break;

            case OT_DEVICE_ROLE_DISABLED:
            case OT_DEVICE_ROLE_DETACHED:
            default:
                thread_coap_utils_provisioning_enable_set(false);
                break;
        }
    }

    NRF_LOG_INFO("State changed! Flags: 0x%08x Current role: %d\r\n", flags, otThreadGetDeviceRole(p_context));
}

/***************************************************************************************************
 * @section Buttons
 **************************************************************************************************/

static void bsp_event_handler(bsp_event_t event)
{
    switch (event)
    {
        case BSP_EVENT_KEY_3:
            thread_coap_utils_provisioning_enable_set(true);
            break;

        default:
            return;
    }
}

/***************************************************************************************************
 * @section Initialization
 **************************************************************************************************/

 /**@brief Function for initializing the Application Timer Module
 */
static void timer_init(void)
{
    uint32_t error_code = app_timer_init();
    APP_ERROR_CHECK(error_code);
}


/**@brief Function for initializing the Thread Board Support Package
 */
static void thread_bsp_init(void)
{
    uint32_t error_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(error_code);

    error_code = bsp_thread_init(thread_ot_instance_get());
    APP_ERROR_CHECK(error_code);
}


/**@brief Function for initializing the Thread Stack
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
    thread_state_changed_callback_set(thread_state_changed_callback);
}


/**@brief Function for initializing the Constrained Application Protocol Module
 */
static void thread_coap_init(void)
{
    thread_coap_utils_configuration_t thread_coap_configuration =
    {
        .coap_server_enabled               = true,
        .coap_client_enabled               = false,
        .configurable_led_blinking_enabled = true,
    };

    thread_coap_utils_init(&thread_coap_configuration);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing the clock.
 */
static void clock_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
}


static void thread_stack_task(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (1)
    {
        thread_process();
        UNUSED_RETURN_VALUE(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
    }
}

/***************************************************************************************************
 * @section Leds
 **************************************************************************************************/

static void led1_task(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (1)
    {
        LEDS_INVERT(BSP_LED_2_MASK);
        vTaskDelay(LED1_BLINK_INTERVAL);
    }
}


static void led2_task(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (1)
    {
        LEDS_INVERT(BSP_LED_3_MASK);
        vTaskDelay(LED2_BLINK_INTERVAL);
    }
}

/***************************************************************************************************
 * @section Idle hook
 **************************************************************************************************/

/**@brief A function which is hooked to idle task.
 * @note Idle hook must be enabled in FreeRTOS configuration (configUSE_IDLE_HOOK).
 */
void vApplicationIdleHook( void )
{
#if NRF_LOG_ENABLED
    if (m_logger_task)
    {
        vTaskResume(m_logger_task);
    }
#endif
}

#if NRF_LOG_ENABLED
/**@brief Task for handling the logger.
 *
 * @details This task is responsible for processing log entries if logs are deferred.
 *          Task flushes all log entries and suspends. It is resumed by idle task hook.
 *
 * @param[in]   arg   Pointer used for passing some arbitrary information (context) from the
 *                    osThreadCreate() call to the task.
 */
static void logger_task(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (1)
    {
        NRF_LOG_FLUSH();

        // Suspend logger task.
        vTaskSuspend(NULL);
    }
}
#endif // NRF_LOG_ENABLED


/***************************************************************************************************
 * @section Main
 **************************************************************************************************/

int main(void)
{
    log_init();
    clock_init();
    timer_init();

#ifdef MBEDTLS_THREADING
    freertos_mbedtls_mutex_init();
#endif

    thread_instance_init();
    thread_coap_init();
    thread_bsp_init();
    thread_coap_utils_light_command_handler_set(on_light_change);

    // Start thread stack execution.
    if (pdPASS != xTaskCreate(thread_stack_task, "THR", THREAD_STACK_TASK_STACK_SIZE, NULL, 2, &m_app.thread_stack_task))
    {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }

#if NRF_LOG_ENABLED
    // Start execution.
    if (pdPASS != xTaskCreate(logger_task, "LOGGER", LOG_TASK_STACK_SIZE, NULL, 1, &m_logger_task))
    {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
#endif

    // Start execution.
    if (pdPASS != xTaskCreate(led1_task, "LED1", configMINIMAL_STACK_SIZE, NULL, 1, &m_app.led1_task))
    {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }

    // Start execution.
    if (pdPASS != xTaskCreate(led2_task, "LED2", configMINIMAL_STACK_SIZE, NULL, 1, &m_app.led2_task))
    {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }

    /* Start FreeRTOS scheduler. */
    vTaskStartScheduler();

    while (true)
    {
        /* FreeRTOS should not be here... FreeRTOS goes back to the start of stack
         * in vTaskStartScheduler function. */
    }
}


/**
 *@}
 **/
