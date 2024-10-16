
#include "i2c_conf.h"
//#include "main.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define MAX_LEN						(0x100)

uint32_t g_u32Slave_buff_addr = 0;
uint8_t g_u8SlvRxData[16] ={0};
uint8_t g_u8SlvData[MAX_LEN] ={0};
uint8_t g_u8SlvDataLen = 0;

/*---------------------------------------------------------------------------------------------------------*/
/*  I2C IRQ Handler                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/

void I2Cx_SlaveTRx(uint32_t u32Status);

unsigned char reg_write_check(uint32_t addr)
{
	unsigned char res = 0;
	unsigned char val = addr - 3;

	switch(val)
	{
		case 0x04:
			res = 1;
			break;
	}

	return res;
}

void I2Cx_Slave_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(SLAVE_I2C);

    if (I2C_GET_TIMEOUT_FLAG(SLAVE_I2C))
    {
		printf("Clear I2C Timeout Flag\r\n");
	
        /* Clear I2C Timeout Flag */
        I2C_ClearTimeoutFlag(SLAVE_I2C); 

        /* Release bus and let slave back to default state */
        SLAVE_I2C->BUSTCTL |= I2C_BUSTCTL_TORSTEN_Msk;
        SLAVE_I2C->BUSTCTL &= ~I2C_BUSTCTL_TORSTEN_Msk;

    }    
    else
    {
		I2Cx_SlaveTRx(u32Status);
    }
}

void I2Cx_SlaveTRx(uint32_t u32Status)
{
	uint8_t u8data = 0;

	// printf("Status:0x%2X\r\n" ,u32Status );

	switch(u32Status)
	{
		case SLAVE_RECEIVE_ADDRESS_ACK://0x60                    			/* Own SLA+W has been receive; ACK has been return */

			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			g_u8SlvDataLen = 0;			
			break;

		case SLAVE_RECEIVE_DATA_ACK://0x80                 					/* Previously address with own SLA address
																			/* Data has been received; ACK has been returned*/
			
			u8data = (unsigned char)I2C_GET_DATA(SLAVE_I2C);
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			if (g_u8SlvDataLen < 1)
			{
				g_u8SlvRxData[g_u8SlvDataLen] = u8data;
				g_u8SlvDataLen++;
				g_u32Slave_buff_addr = g_u8SlvRxData[0];
				g_u32Slave_buff_addr += 3;
				
				// printf("slv1:0x%02X\r\n",u8data);
			}
			else
			{
				// printf("slv2:0x%02X\r\n",u8data);
				if (reg_write_check(g_u32Slave_buff_addr) == 1)
					g_u8SlvData[g_u32Slave_buff_addr--] = u8data;
			}
			break;

		case SLAVE_TRANSMIT_ADDRESS_ACK://0xA8               				/* Own SLA+R has been receive; ACK has been return */
					
			I2C_SET_DATA(SLAVE_I2C, g_u8SlvData[g_u32Slave_buff_addr--]);
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			break;

		case SLAVE_TRANSMIT_DATA_ACK://0xB8                 				/* Data byte in I2CDAT has been transmitted
																			/* ACK has been received */				
			
			I2C_SET_DATA(SLAVE_I2C, g_u8SlvData[g_u32Slave_buff_addr--]);
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);			
			break;

		case SLAVE_TRANSMIT_DATA_NACK://0xC0                 				/* Data byte or last data in I2CDAT has been transmitted
																			/* Not ACK has been received */
		
		    I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);	

			// printf("\r\n");

			break;

		case SLAVE_RECEIVE_DATA_NACK://0x88                 				/* Previously addressed with own SLA address; NOT ACK has
																			/* been returned */	

		    I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			g_u8SlvDataLen = 0;
			break;

		case SLAVE_TRANSMIT_REPEAT_START_OR_STOP://0xA0    					/* A STOP or repeated START has been received while still
																			/* addressed as Slave/Receiver*/		

		    I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			g_u8SlvDataLen = 0;
			
			// printf("\r\n");

			break;

		default :
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);

			printf("%s Status 0x%x is NOT processed\n", __FUNCTION__ , u32Status);
			break;


	}
	
}

void I2Cx_Slave_Init(void)
{    
    uint32_t i = 0;

    /* Open I2C module and set bus clock */
    I2C_Open(SLAVE_I2C, 400000);
    
     /* Set I2C 4 Slave Addresses */            
    I2C_SetSlaveAddr(SLAVE_I2C, 0, 0x58, 0);   /* Slave Address : 0x58 */

    /* Enable I2C interrupt */
    I2C_EnableInt(SLAVE_I2C);
    NVIC_EnableIRQ(SLAVE_I2C_IRQn);

	/* I2C enter no address SLV mode */
    I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);	

	for(i = 0; i < 16; i++)
	{
		g_u8SlvRxData[i] = 0;
	}
	for(i = 0; i < 0x100; i++)
	{
		g_u8SlvData[i] = 0;
	}
	
}


