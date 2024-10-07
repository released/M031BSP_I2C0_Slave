
#include "i2c_conf.h"
//#include "main.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define MAX_LEN						(0x100)

#define INVALID_REG					(0x7A)

uint32_t slave_buff_addr = 0;
uint8_t g_u8SlvData[MAX_LEN] ={0};

uint8_t g_u8DataLen_s = 0;

uint8_t g_u8ToMasterLen = 0;
uint8_t g_u8ToMasterData[64] ={0};
uint8_t g_u8FromMasterLen = 0;
uint8_t g_u8FromMasterData[MAX_LEN] ={0};

uint8_t slave_register_addr = 0;
uint8_t g_u8temporary = 0;

typedef void (*I2C_FUNC)(uint32_t u32Status);

I2C_FUNC __IO I2Cx_Slave_HandlerFn = NULL;

void I2Cx_SlaveTRx(uint32_t u32Status);

enum
{
	_state_DEFAULT_ = 0 , 
	_state_RECEIVE_ADDRESS_ = 1 ,
	_state_CHECK_RX_OR_TX_ = 2 ,
	_state_RECEIVE_RX_ = 3 ,	
	_state_TRANSMIT_TX_ = 4 ,	
};

/*---------------------------------------------------------------------------------------------------------*/
/*  I2C IRQ Handler                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/
void __I2Cx_Slave_LogBuffer__(uint8_t str[] ,uint8_t log1 , uint8_t log2)
{
	#if defined (DEBUG_LOG_SLAVE_LV2)
	printf("%sslave-log1:0x%02X ,log2:0x%02X\r\n" , str , log1 , log2);
	#endif	
}

void __I2Cx_Slave_Log__(uint32_t u32Status)
{
	#if defined (DEBUG_LOG_SLAVE_LV1)
    printf("slave-Status [0x%02X] is processed\n", u32Status);
	#endif
}

void __I2Cx_Slave_StateMachine_Log__(uint8_t log1 , uint8_t log2)
{
	#if defined (DEBUG_LOG_SLAVE_STATEMACHINE_LV1)
    printf("slave-STATEMACHINE [0x%02X] , 0x%2X\r\n", log1,log2);
	#endif
}

void I2Cx_Slave_ReturnTx(void)
{
	uint8_t i = 0;	

	// clear tx buffer
	for (i = 0; i <64 ; i++)
	{
		g_u8ToMasterData[i] = 0x00;
	}
	#if 1
	for (i = 0; i < g_u8temporary; i++)
	{
		g_u8ToMasterData[i] = g_u8FromMasterData[i];
//		printf("From : 0x%2X, To : 0x%2X,\r\n" , g_u8FromMasterData[i],g_u8ToMasterData[i]);
	}

	#else
	// swap the data 
	for (i = 0; i < g_u8temporary; i++)
	{
		g_u8ToMasterData[(g_u8temporary-1)-i] = g_u8FromMasterData[i];
//		printf("From : 0x%2X, To : 0x%2X,\r\n" , g_u8FromMasterData[i],g_u8ToMasterData[i]);
	}
	#endif

	g_u8FromMasterLen = 0;
	g_u8ToMasterLen = 0;
	g_u8temporary = 0;
}

void I2Cx_Slave_StateMachine(uint32_t res , uint8_t* InData, uint8_t* OutData)
{
//	uint8_t i = 0;	
//	static uint8_t u8Temp = 0;
	static uint8_t cnt = _state_DEFAULT_;	

	if (res == SLAVE_RECEIVE_ADDRESS_ACK)	//no this ack when not receive register address
	{
		cnt = _state_RECEIVE_ADDRESS_;
		slave_register_addr = 0;
		g_u8FromMasterLen = 0;
		g_u8ToMasterLen = 0;		
	}

	switch(cnt)
	{
		case _state_RECEIVE_ADDRESS_:
			if (res == SLAVE_RECEIVE_DATA_ACK)	
			{
				// first 0x80 ack is register address
				if(g_u8DataLen_s == 1)
				{
//					set_flag(flag_state_RECEIVE_ADDRESS_ , ENABLE);

					slave_register_addr = slave_buff_addr;

					if (slave_register_addr == INVALID_REG)
					{						
						cnt = _state_DEFAULT_;
						return;
					}
				}
				cnt = _state_CHECK_RX_OR_TX_;
			}
			else if (res ==  SLAVE_RECEIVE_DATA_NACK)
			{
				cnt = _state_DEFAULT_;	//reset flag
			}
			
			__I2Cx_Slave_StateMachine_Log__(res , cnt);
			break;

		case _state_CHECK_RX_OR_TX_:
			if (res == SLAVE_RECEIVE_DATA_ACK)	
			{
				// RX : contiune with multi 0x80 , end with 0xA0
				// example : 0x60 > 0x80 (address) > 0x80 (data00)> 0x80 (data01) > ...> 0xA0

				if (slave_register_addr == INVALID_REG)
				{						
					cnt = _state_DEFAULT_;
					return;
				}

				g_u8FromMasterData[g_u8FromMasterLen++] =  *InData;
				cnt = _state_RECEIVE_RX_;
			}
			else if (res == SLAVE_TRANSMIT_REPEAT_START_OR_STOP)	
			{
				// if use I2C_ReadMultiBytesOneReg , 
				// ack will act as below , 0x60 > 0x80 > 0xA0 > 0xA8 > 0xB8
				// TX : following with 0xA0 , and 0xA8 (data01) , continue with 0xB8 (Data02)  , end with 0xC0
				// example : 0x60 > 0x80 (address) > 0xA8 (data00) > 0xB8  (data01)> 0xB8  (data02) > ... > 0xC0
				
				if (slave_register_addr == INVALID_REG)
				{						
					cnt = _state_DEFAULT_;
					return;
				}

				cnt = _state_TRANSMIT_TX_;				
			}
			
			__I2Cx_Slave_StateMachine_Log__(res , cnt);
			break;

		case _state_RECEIVE_RX_:
			if (res == SLAVE_TRANSMIT_REPEAT_START_OR_STOP)	
			{
				// end of RX

				if (slave_register_addr == INVALID_REG)
				{						
					cnt = _state_DEFAULT_;
					return;
				}

				g_u8temporary = g_u8FromMasterLen;

				// if use I2C_ReadMultiBytes , 
				// ack will act as below , 0xA8 > 0xB8 > 0xB8 > ... > 0xC0
				// so change state machine here
				// must follow with 
				// 1) I2C_WriteMultiBytes , I2C_ReadMultiBytes , or
				// 2) I2C_WriteMultiBytesOneReg , , I2C_ReadMultiBytes
				// 3) directlly by using I2C_ReadMultiBytes , will be no data transfer
				
//				cnt = _state_DEFAULT_;	//reset flag
				cnt = _state_TRANSMIT_TX_;
				
			}
			else if (res == SLAVE_RECEIVE_DATA_ACK)
			{
				// continue to get data
				g_u8FromMasterData[g_u8FromMasterLen++] =  *InData;
			}
			
			__I2Cx_Slave_StateMachine_Log__(res , cnt);
			break;

		case _state_TRANSMIT_TX_:
			if (res == SLAVE_TRANSMIT_ADDRESS_ACK)	
			{
				I2Cx_Slave_ReturnTx();
				
				// first TX byte				
				*OutData = g_u8ToMasterData[g_u8ToMasterLen++];
			}
			else if (res == SLAVE_TRANSMIT_DATA_NACK)	
			{
				// end of TX
//				set_flag(flag_SLAVE_TRANSMIT_DATA_NACK , ENABLE);
				cnt = _state_DEFAULT_;	//reset flag
			}
			else if (res == SLAVE_TRANSMIT_DATA_ACK)
			{
				// continue to send data
				*OutData = g_u8ToMasterData[g_u8ToMasterLen++];
			}
			
			__I2Cx_Slave_StateMachine_Log__(res , cnt);
			break;			
	}
}

void I2Cx_Slave_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(SLAVE_I2C);
#if 1
    if (I2C_GET_TIMEOUT_FLAG(SLAVE_I2C))
    {
//		printf("Clear I2C Timeout Flag\r\n");
	
        /* Clear I2C Timeout Flag */
        I2C_ClearTimeoutFlag(SLAVE_I2C); 

        /* Disable time-out */
		#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
//        SLAVE_I2C->TOCTL &= ~I2C_TOCTL_TOCEN_Msk;
		I2C_DisableTimeout(SLAVE_I2C);
		#endif
        
        /* Release bus and let slave back to default state */
        SLAVE_I2C->BUSTCTL |= I2C_BUSTCTL_TORSTEN_Msk;
        SLAVE_I2C->BUSTCTL &= ~I2C_BUSTCTL_TORSTEN_Msk;

    }    
    else
#endif
    {
        if (I2Cx_Slave_HandlerFn != NULL)
        {
			I2Cx_Slave_HandlerFn(u32Status);
        }

    }
}

/*

//RX
Status 0x60 is processed
0x 1 : 0x66
Status 0x80 is processed
0x66 : 0x12
Status 0x80 is processed
0x67 : 0x34
Status 0x80 is processed
0x68 : 0x56
Status 0x80 is processed
0x69 : 0x78
Status 0x80 is processed
0x6A : 0x90
Status 0x80 is processed
0x6B : 0xAB
Status 0x80 is processed
0x6C : 0xCD
Status 0x80 is processed
0x6D : 0xEF
Status 0x80 is processed
0x6E : 0x99
Status 0x80 is processed
0x6F : 0xFE
Status 0x80 is processed
Status 0xa0 is processed


//TX(below ack : with master send readRX with register)
//if no register , ack will start with 0xA8

Status 0x60 is processed
0x 1 : 0x66
Status 0x80 is processed
Status 0xa0 is processed

0xA8 : 0x12
Status 0xa8 is processed
0xB8 : 0x34
Status 0xb8 is processed
0xB8 : 0x56
Status 0xb8 is processed
0xB8 : 0x78
Status 0xb8 is processed
0xB8 : 0x90
Status 0xb8 is processed
0xB8 : 0xAB
Status 0xb8 is processed
0xB8 : 0xCD
Status 0xb8 is processed
0xB8 : 0xEF
Status 0xb8 is processed
0xB8 : 0x99
Status 0xb8 is processed
0xB8 : 0xFE
Status 0xb8 is processed
Status 0xc0 is processed




*/

void I2Cx_SlaveTRx(uint32_t u32Status)
{
	uint8_t u8RxData = 0;
	uint8_t u8TxData = 0;
	uint8_t u8TempData = 0;	
	uint16_t u16Rxlen = 0;

//	printf("I2Cx_SlaveTRx : 0x%2X\r\n" ,u32Status );

	switch(u32Status)
	{
		case SLAVE_RECEIVE_ADDRESS_ACK://0x60                    			/* Own SLA+W has been receive; ACK has been return */

			g_u8DataLen_s = 0;
			I2Cx_Slave_StateMachine(u32Status , &u8TempData , &u8TempData);

			/* Enable time-out */
			#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
	       	// SLAVE_I2C->TOCTL |= I2C_TOCTL_TOCEN_Msk;
			I2C_EnableTimeout(SLAVE_I2C, 0);
			#endif
			
			#if 0
			// I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI);

			if (u8TempData == INVALID_REG)
			{
				I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI);
				printf("SLAVE_RECEIVE_ADDRESS_ACK-INCORRECT\r\n");
			}
			else
			{
				I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
				printf("SLAVE_RECEIVE_ADDRESS_ACK-\r\n");
			}
			#else
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
			#endif

			__I2Cx_Slave_LogBuffer__("",u32Status , 0);
			__I2Cx_Slave_Log__(u32Status);				
			break;

		case SLAVE_RECEIVE_DATA_ACK://0x80                 					/* Previously address with own SLA address
																			/* Data has been received; ACK has been returned*/
			u8RxData = (unsigned char) I2C_GET_DATA(SLAVE_I2C);
			
			g_u8DataLen_s++;

			if(g_u8DataLen_s == 1)
			{
				/*
					Blind spot : register address can not larger than g_u8SlvData array size , 
					due to resister address will be save as array start index to store data
				
				*/
				slave_buff_addr = u8RxData;	//register
				I2Cx_Slave_StateMachine(u32Status , &u8RxData , &u8TempData);
				
				__I2Cx_Slave_LogBuffer__("[register]",g_u8DataLen_s , u8RxData);	

				if (slave_register_addr == INVALID_REG)
				{						
					I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI);
					return;
				}
							
				I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
				
				__I2Cx_Slave_Log__(u32Status);

			}
			else
			{
				// if (slave_register_addr == INVALID_REG)
				// {						
				// 	I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI);
				// 	return;
				// }

				u16Rxlen = slave_buff_addr+(g_u8DataLen_s-2);		//data buffer start	
				g_u8SlvData[u16Rxlen] = u8RxData;
				
				I2Cx_Slave_StateMachine(u32Status , &u8RxData , &u8TempData);						
				I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);

				__I2Cx_Slave_LogBuffer__("[rx data]",u16Rxlen , u8RxData);
				__I2Cx_Slave_Log__(u32Status);
			}	

			break;

		case SLAVE_TRANSMIT_ADDRESS_ACK://0xA8               				/* Own SLA+R has been receive; ACK has been return */
			#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
			I2C_EnableTimeout(SLAVE_I2C, 0);
			#endif
			
			u8RxData = g_u8SlvData[slave_buff_addr++];
			I2Cx_Slave_StateMachine(u32Status , &u8RxData, &u8TxData);	

			/* Enable time-out */
	    //    SLAVE_I2C->TOCTL |= I2C_TOCTL_TOCEN_Msk;		

			// I2C_SET_DATA(SLAVE_I2C, u8RxData);
			I2C_SET_DATA(SLAVE_I2C, u8TxData);	
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);

			__I2Cx_Slave_LogBuffer__("[tx data1]",u32Status , u8TxData);
			__I2Cx_Slave_Log__(u32Status);
			break;

		case SLAVE_TRANSMIT_DATA_ACK://0xB8                 				/* Data byte in I2CDAT has been transmitted
																			/* ACK has been received */				
			u8RxData = g_u8SlvData[slave_buff_addr++];
			I2Cx_Slave_StateMachine(u32Status , &u8RxData, &u8TxData);

			// I2C_SET_DATA(SLAVE_I2C, u8RxData);
			I2C_SET_DATA(SLAVE_I2C, u8TxData);	
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
			
			__I2Cx_Slave_LogBuffer__("[tx data2]",u32Status, u8TxData);		
			__I2Cx_Slave_Log__(u32Status);
			break;

		case SLAVE_TRANSMIT_DATA_NACK://0xC0                 				/* Data byte or last data in I2CDAT has been transmitted
																			/* Not ACK has been received */
			I2Cx_Slave_StateMachine(u32Status , &u8TempData , &u8TempData);
			
			#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
			I2C_DisableTimeout(SLAVE_I2C);
			#endif
		
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);

			__I2Cx_Slave_LogBuffer__("",u32Status, 0);	
			__I2Cx_Slave_Log__(u32Status);
			break;

		case SLAVE_RECEIVE_DATA_NACK://0x88                 				/* Previously addressed with own SLA address; NOT ACK has
																			/* been returned */	
			g_u8DataLen_s = 0;
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
				
			__I2Cx_Slave_LogBuffer__("",u32Status, u8TempData);	
			__I2Cx_Slave_Log__(u32Status);
			break;

		case SLAVE_TRANSMIT_REPEAT_START_OR_STOP://0xA0    					/* A STOP or repeated START has been received while still
																			/* addressed as Slave/Receiver*/		
			g_u8DataLen_s = 0;
			I2Cx_Slave_StateMachine(u32Status , &u8TempData, &u8TempData);

			/* Disable time-out */
			#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
		//    SLAVE_I2C->TOCTL &= ~I2C_TOCTL_TOCEN_Msk;
			I2C_DisableTimeout(SLAVE_I2C);
			#endif
			
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);
				
			__I2Cx_Slave_LogBuffer__("",u32Status, 0);
			__I2Cx_Slave_Log__(u32Status);
			break;

		// case BUS_ERROR://0x00
		// 	/* Disable Time-out */
		// 	#if defined (SLAVE_I2C_TIMEOUT_ENABLE)
		// //    SLAVE_I2C->TOCTL &= ~I2C_TOCTL_TOCEN_Msk;
		// 	I2C_DisableTimeout(SLAVE_I2C);
		// 	#endif

		// 	/* Recover bus */
		// 	I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_STO_SI);
		// 	I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI);
			
		// 	/* Back to not addressed mode */
		// 	I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);
			
		// 	__I2Cx_Slave_LogBuffer__("",u32Status, 0);
		// 	__I2Cx_Slave_Log__(u32Status);	
		// 	break;

		default :
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI);
			I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI_AA);

			// #if defined (DEBUG_LOG_SLAVE_LV2)
			/* TO DO */
			printf("I2Cx_SlaveTRx Status 0x%x is NOT processed\n", u32Status);
			// #endif
			break;


	}
	
}

void I2Cx_Slave_Init(void)
{    
    uint32_t i;

    /* Open I2C module and set bus clock */
    I2C_Open(SLAVE_I2C, 400000);
    
     /* Set I2C 4 Slave Addresses */            
    I2C_SetSlaveAddr(SLAVE_I2C, 0, 0x58, 0);   /* Slave Address : 0x58 */

    /* Enable I2C interrupt */
    I2C_EnableInt(SLAVE_I2C);
    NVIC_EnableIRQ(SLAVE_I2C_IRQn);

	/* I2C enter no address SLV mode */
    I2C_SET_CONTROL_REG(SLAVE_I2C, I2C_CTL_SI | I2C_CTL_AA);	

	for(i = 0; i < 0x100; i++)
	{
		g_u8SlvData[i] = 0;
	}

	I2Cx_Slave_HandlerFn = I2Cx_SlaveTRx;
	
}


