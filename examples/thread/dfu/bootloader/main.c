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
 * @defgroup bootloader_secure main.c
 * @{
 * @ingroup dfu_bootloader_api
 * @brief Bootloader project main file for secure DFU.
 *
 */

#include <stdint.h>
#include "boards.h"
#include "nrf_bootloader.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_soc.h"
#include "nrf_dfu.h"
#include "nrf_mbr.h"
#include "app_error.h"
#include "app_error_weak.h"
#include "nrf_bootloader_info.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_dfu_mbr.h"

#define LEGACY_UICR_BOOTLOADER_START_ADDRESS 0x10001014  // Location (in UICR) of the bootloader address.

/** @brief  This variable ensures that the linker script will write the bootloader start address
 *          to the UICR register. This value will be written in the HEX file and thus written to
 *          UICR when the bootloader is flashed into the chip.
 *
 *          New bootloader writes its start address also in MBR code region. This operation ensures
 *          backward compatibility.
 */
#if defined (__CC_ARM )
    #pragma push
    #pragma diag_suppress 1296
    uint32_t  m_legacy_uicr_bootloader_start_address __attribute__((at(LEGACY_UICR_BOOTLOADER_START_ADDRESS)))
                                                           = BOOTLOADER_START_ADDR;
    #pragma pop
#elif defined ( __GNUC__ ) || defined ( __SES_ARM )
    volatile uint32_t m_legacy_uicr_bootloader_start_address __attribute__ ((section(".legacy_uicr_bootloader_start_address")))
                                                   = BOOTLOADER_START_ADDR;
#elif defined ( __ICCARM__ )
    __root    const uint32_t m_legacy_uicr_bootloader_start_address @ LEGACY_UICR_BOOTLOADER_START_ADDRESS
                                                   = BOOTLOADER_START_ADDR;
#endif

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    NRF_LOG_ERROR("received a fault! id: 0x%08x, pc: 0x&08x\r\n", id, pc);
    NVIC_SystemReset();
}

void app_error_handler_bare(uint32_t error_code)
{
    NRF_LOG_ERROR("received an error: 0x%08x!\r\n", error_code);
    NVIC_SystemReset();
}

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    NRF_LOG_ERROR("received an error: 0x%08x, line: 0x%lu, file: %s\r\n", error_code, line_num, p_file_name);
    NVIC_SystemReset();
}

/**
 * @brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**
 * @brief Function for initialization of LEDs.
 */
static void leds_init(void)
{
    bsp_board_init(BSP_INIT_LEDS);
}

/**
 * @brief Function notifies certain events in DFU process.
 */
static void dfu_observer(nrf_dfu_evt_type_t evt_type)
{
    switch (evt_type)
    {
        case NRF_DFU_EVT_DFU_INITIALIZED:
            bsp_board_led_on(BSP_BOARD_LED_2);
            break;

        default:
            break;
    }
}

/**@brief Function for application main entry.
 */
int main(void)
{
    uint32_t ret_val;

    log_init();

    NRF_LOG_INFO("Inside main\r\n");

    leds_init();

    ret_val = nrf_bootloader_init(dfu_observer);
    APP_ERROR_CHECK(ret_val);

    // Either there was no DFU functionality enabled in this project or the DFU module detected
    // no ongoing DFU operation and found a valid main application.
    // Boot the main application.
    nrf_bootloader_app_start();

    // Should never be reached.
    NRF_LOG_INFO("After main\r\n");
}

/**
 * @}
 */
