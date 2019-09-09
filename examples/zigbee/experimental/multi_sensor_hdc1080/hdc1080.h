#ifndef HDC1080_H__
#define HDC1080_H__

    #include "nrf_drv_twi.h"
    #include "app_error.h" //for APP_ERROR_CHECK(err_code);
		#include "nrf_delay.h"
    #ifdef DEBUG_MODE
		#include "SEGGER_RTT.h"
		#endif

#ifdef __cplusplus
extern "C" {
#endif

    uint8_t buff[2];
    
    void I2C_init(void);
    void hdc1080_init(void);
    uint16_t hdc1080_get_temp(void);
    uint16_t hdc1080_get_hum(void);
    void I2C_handler(nrf_drv_twi_evt_t const * p_event, void * p_context);

#ifdef __cplusplus
}
#endif

#endif // HDC1080_H__
