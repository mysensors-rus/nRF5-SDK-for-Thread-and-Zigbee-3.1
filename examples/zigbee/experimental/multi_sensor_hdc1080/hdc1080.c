
#include "hdc1080.h"
#include "nrf_drv_twi.h"

#define NRF_LOG_MODULE_NAME "I2C"

// I2C instance.
#define TWI_INSTANCE_ID 1

static const nrf_drv_twi_t i2c = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

// Indicates if operation on TWI has ended.
static volatile bool m_xfer_done = false;

//--------------- HDC1080 ---------------------
#define         HDC_1080_ADD                            0x40
#define         Configuration_register_add              0x02U
#define         Temperature_register_add                0x00
#define         Humidity_register_add                   0x01
//------------------------------------------------------------

// @brief UART initialization.
void I2C_init(void)
{        
    ret_code_t err_code;

    const nrf_drv_twi_config_t i2c_config = 
    {
       .scl                = 30,
       .sda                = 31,
       .frequency          = NRF_DRV_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };
		
    //last one is some kind of context - no idea what that is....
    //documentation is vague/nonexistant
    err_code = nrf_drv_twi_init(&i2c, &i2c_config, I2C_handler, NULL);
    APP_ERROR_CHECK(err_code);
    
		nrf_drv_twi_enable(&i2c);
}

/* --------------- START HDC1080 -------------------------------------- */
void hdc1080_init(void)
{
	uint32_t      err_code;
	uint16_t config_reg_value=0x00; //0x1000;
	uint8_t data_send[3];
	config_reg_value |= (1<<10); //11 bit
	config_reg_value|= (1<<8);
	data_send[0] = Configuration_register_add;
	data_send[1]= (config_reg_value>>8);
	data_send[2]= (config_reg_value&0x00ff);
	err_code = nrf_drv_twi_tx(&i2c,HDC_1080_ADD,data_send,3,false);
    nrf_delay_ms(1);

	APP_ERROR_CHECK(err_code);
}


//uint32_t * hdc1080_get(uint8_t* receive_data)
//{
//	ret_code_t err_code;
//        uint32_t mydat;
//
//	uint8_t send_data = Temperature_register_add;
//	err_code = nrf_drv_twi_tx(&i2c,HDC_1080_ADD,&send_data,1,false);
//
//	nrf_delay_ms(15);
//
//	err_code = nrf_drv_twi_rx(&i2c,HDC_1080_ADD,receive_data,4);
//        
////	if (err_code == NRF_SUCCESS)
////	{
////	//NRF_LOG_INFO("data recievd");
////	}
//
//        return receive_data;
//}


uint16_t hdc1080_get_temp()
{
    //uint8_t buff[2];
	uint8_t send_data = Temperature_register_add;
	nrf_drv_twi_tx(&i2c,HDC_1080_ADD,&send_data,1,false);
	nrf_delay_ms(7);
	nrf_drv_twi_rx(&i2c,HDC_1080_ADD,&buff,2);
    nrf_delay_ms(1);
    return buff[0] << 8 | buff[1];
}


uint16_t hdc1080_get_hum()
{
    //uint8_t buff[2];
	uint8_t send_data = Humidity_register_add;
	nrf_drv_twi_tx(&i2c,HDC_1080_ADD,&send_data,1,false);
	nrf_delay_ms(7);
	nrf_drv_twi_rx(&i2c,HDC_1080_ADD,&buff,2);
    nrf_delay_ms(1);
    return buff[0] << 8 | buff[1];
}

// @brief TWI events handler.
void I2C_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:
            
            //todo -difference between read and write???
            m_xfer_done = true;
            
            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
            {
                //SEGGER_RTT_WriteString(0, "Data just came back!\n");  
            }
            
            break;
        default:
            break;
    }
}


