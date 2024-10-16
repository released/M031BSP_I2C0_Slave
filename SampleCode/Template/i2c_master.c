
#include "i2c_conf.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/

volatile uint8_t g_u8DeviceAddr_m;
volatile uint8_t g_u8DataLen_m;
volatile uint8_t rawlenth;
volatile uint8_t g_au8Reg;
volatile uint8_t g_u8EndFlag = 0;
uint8_t *g_au8Buffer;

uint8_t test_case_reg = 0x00;
uint8_t test_case_len = 0x02;
uint8_t test_case_lsb = 0x3C;
uint8_t test_case_msb = 0x00;

typedef void (*I2C_FUNC)(uint32_t u32Status);

I2C_FUNC __IO I2Cx_Master_HandlerFn = NULL;


#define SET_SCL								(PC5 = ENABLE)
#define RESET_SCL							(PC5 = DISABLE)
#define SET_SDA								(PC4 = ENABLE)
#define RESET_SDA							(PC4 = DISABLE)

/*---------------------------------------------------------------------------------------------------------*/
/*  I2C IRQ Handler                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/

uint8_t CRC_Get(uint8_t* data , uint16_t len)
{
	uint16_t i = 0;
	uint32_t u32CalChecksum = 0;
	
    CRC_Open(CRC_8, 0, 0x5A, CRC_WDATA_8);

    /* Start to execute CRC-8 CPU operation */
    for(i = 0; i < len ; i++)
    {
        CRC_WRITE_DATA(data[i]);
    }

    /* Get CRC-8 checksum value */
    u32CalChecksum = CRC_GetChecksum();

//    /* Disable CRC function */
//    CLK_DisableModuleClock(CRC_MODULE);

	return u32CalChecksum;
}

void I2Cx_Master_LOG(uint32_t u32Status)
{
	#if defined (DEBUG_LOG_MASTER_LV1)
    printf("master : Status 0x%x is processed\n", u32Status);
	#endif
}

void I2Cx_Master_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(MASTER_I2C);

    if (I2C_GET_TIMEOUT_FLAG(MASTER_I2C))
    {
        /* Clear I2C Timeout Flag */
        I2C_ClearTimeoutFlag(MASTER_I2C);     

		g_u8EndFlag = 1;

		/* Reset and initiate I2C again*/
		I2C_Close(MASTER_I2C);

		SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC4MFP_Msk)) |
						(SYS_GPC_MFPL_PC5MFP_I2C1_SCL | SYS_GPC_MFPL_PC4MFP_I2C1_SDA);		
		/* Open I2C module and set bus clock */
		I2C_Open(MASTER_I2C, 100000);          
    }    
    else
    {
        if (I2Cx_Master_HandlerFn != NULL)
            I2Cx_Master_HandlerFn(u32Status);
    }
}

void I2Cx_MasterRx_multi(uint32_t u32Status)
{
	switch(u32Status)
	{
		case MASTER_START_TRANSMIT://0x08 									/* START has been transmitted and prepare SLA+W */
			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_EnableTimeout(MASTER_I2C, 0);
			#endif

			I2C_SET_DATA(MASTER_I2C, g_u8DeviceAddr_m << 1);    				/* Write SLA+W to Register I2CDAT */
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);

			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_TRANSMIT_ADDRESS_ACK://0x18        						/* SLA+W has been transmitted and ACK has been received */
			I2C_SET_DATA(MASTER_I2C, g_au8Reg);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_TRANSMIT_DATA_ACK://0x28                  				/* DATA has been transmitted and ACK has been received */
			#if 1
			if (rawlenth > 0)
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA);	//repeat start
			else
			{
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
				g_u8EndFlag = 1;
			}
			#else
			if(g_u8DataLen_m < rawlenth)
			{
				I2C_SET_DATA(MASTER_I2C, g_au8Buffer[g_u8DataLen_m++]);
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			}
			else
			{
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA);
			
				#if defined (MASTER_I2C_TIMEOUT_ENABLE)
				I2C_DisableTimeout(MASTER_I2C);
				#endif
			}
			#endif
			
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_REPEAT_START://0x10                  					/* Repeat START has been transmitted and prepare SLA+R */
			I2C_SET_DATA(MASTER_I2C, ((g_u8DeviceAddr_m << 1) | 0x01));   		/* Write SLA+R to Register I2CDAT */
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			
			I2Cx_Master_LOG(u32Status);
			break;			

		case MASTER_RECEIVE_ADDRESS_ACK://0x40                				/* SLA+R has been transmitted and ACK has been received */			
			#if 1
			if (rawlenth > 1)
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
			else
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			#else
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
			#endif
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_RECEIVE_DATA_ACK://0x50                 				/* DATA has been received and ACK has been returned */
			g_au8Buffer[g_u8DataLen_m++] = (unsigned char) I2C_GetData(MASTER_I2C);
			if (g_u8DataLen_m < (rawlenth-1))
			{
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
			}
			else
			{
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			}
			
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_RECEIVE_DATA_NACK://0x58                  				/* DATA has been received and NACK has been returned */
			g_au8Buffer[g_u8DataLen_m++] = (unsigned char) I2C_GetData(MASTER_I2C);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);

			g_u8EndFlag = 1;
			
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_TRANSMIT_ADDRESS_NACK://0x20            				/* SLA+W has been transmitted and NACK has been received */
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA | I2C_CTL_STO);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);

			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_DisableTimeout(MASTER_I2C);
			#endif

			// I2C_STOP(MASTER_I2C);
			// I2C_START(MASTER_I2C);

			g_u8EndFlag = 1;
			
			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_TRANSMIT_DATA_NACK://0x30
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA | I2C_CTL_STO);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);

			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_DisableTimeout(MASTER_I2C);
			#endif

			g_u8EndFlag = 1;
			
			I2Cx_Master_LOG(u32Status);	
			break;

		case MASTER_ARBITRATION_LOST: //0x38
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA_SI_AA);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI);

			g_u8EndFlag = 1;

			I2Cx_Master_LOG(u32Status);		
			break;

		case BUS_ERROR: //0x00
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI_AA);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI_AA);

			g_u8EndFlag = 1;
			
			I2Cx_Master_LOG(u32Status);	
			break;

		default:
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI);

			g_u8EndFlag = 1;

			I2C_DisableInt(MASTER_I2C);
			if(MASTER_I2C == I2C0)
				NVIC_DisableIRQ(I2C0_IRQn);
			else
				NVIC_DisableIRQ(I2C1_IRQn); 
		
			// #if defined (DEBUG_LOG_MASTER_LV1)
			/* TO DO */
			printf("I2Cx_MasterRx_multi Status 0x%x is NOT processed\n", u32Status);
			// #endif
			break;
	}

}

void I2Cx_MasterTx_multi(uint32_t u32Status)
{
	switch(u32Status)
	{
		case MASTER_START_TRANSMIT://0x08                     				/* START has been transmitted */
			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_EnableTimeout(MASTER_I2C, 0);
			#endif

			I2C_SET_DATA(MASTER_I2C, g_u8DeviceAddr_m << 1);    			/* Write SLA+W to Register I2CDAT */
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);

			I2Cx_Master_LOG(u32Status);
			break;

		case MASTER_TRANSMIT_ADDRESS_ACK://0x18           					/* SLA+W has been transmitted and ACK has been received */
			I2C_SET_DATA(MASTER_I2C, g_au8Reg);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			
			I2Cx_Master_LOG(u32Status);	
			break;

		case MASTER_TRANSMIT_DATA_ACK://0x28              					/* DATA has been transmitted and ACK has been received */
			if(g_u8DataLen_m < rawlenth)
			{
				I2C_SET_DATA(MASTER_I2C, g_au8Buffer[g_u8DataLen_m++]);
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
			}
			else
			{
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
			
				#if defined (MASTER_I2C_TIMEOUT_ENABLE)
				I2C_DisableTimeout(MASTER_I2C);
				#endif
				g_u8EndFlag = 1;
			}

			I2Cx_Master_LOG(u32Status);		
			break;

		case MASTER_TRANSMIT_ADDRESS_NACK://0x20           					/* SLA+W has been transmitted and NACK has been received */ 
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA | I2C_CTL_STO);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
				
			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_DisableTimeout(MASTER_I2C);
			#endif

			// I2C_STOP(MASTER_I2C);
			// I2C_START(MASTER_I2C);

			g_u8EndFlag = 1;

			I2Cx_Master_LOG(u32Status);			
			break;

		case MASTER_TRANSMIT_DATA_NACK://0x30
			// I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA | I2C_CTL_STO);	
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
			
			#if defined (MASTER_I2C_TIMEOUT_ENABLE)
			I2C_DisableTimeout(MASTER_I2C);
			#endif

			g_u8EndFlag = 1;

			I2Cx_Master_LOG(u32Status);	
			break;

		case MASTER_ARBITRATION_LOST: //0x38
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA_SI_AA);

			g_u8EndFlag = 1;

			I2Cx_Master_LOG(u32Status);		
			break;

		case BUS_ERROR: //0x00
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI_AA);
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI_AA);

			g_u8EndFlag = 1;
			
			I2Cx_Master_LOG(u32Status);	
			break;

		default:
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI);

			g_u8EndFlag = 1;

			I2C_DisableInt(MASTER_I2C);
			if(MASTER_I2C == I2C0)
				NVIC_DisableIRQ(I2C0_IRQn);
			else
				NVIC_DisableIRQ(I2C1_IRQn);
		
			// #if defined (DEBUG_LOG_MASTER_LV1)
			/* TO DO */
			printf("I2Cx_MasterTx_multi Status 0x%x is NOT processed\n", u32Status);
			// #endif
			break;
	}
}

void I2Cx_Write_Multi_ToSlave(uint8_t address,uint8_t reg,uint8_t *data,uint16_t len)
{		
	g_u8DeviceAddr_m = address;
	rawlenth = len;
	g_au8Reg = reg;
	g_au8Buffer = data;

	g_u8DataLen_m = 0;
	g_u8EndFlag = 0;

	/* I2C function to write data to slave */
	I2Cx_Master_HandlerFn = (I2C_FUNC) I2Cx_MasterTx_multi;

//	printf("I2Cx_MasterTx_multi finish\r\n");

	/* I2C as master sends START signal */
	I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA);
		
	#if defined (MASTER_I2C_TIMEOUT_ENABLE)
	I2C_EnableTimeout(MASTER_I2C, 0);
	#endif

	/* Wait I2C Tx Finish */
	while(g_u8EndFlag == 0);
	g_u8EndFlag = 0;
}

void I2Cx_Read_Multi_FromSlave(uint8_t address,uint8_t reg,uint8_t *data,uint16_t len)
{ 
	g_u8DeviceAddr_m = address;
	rawlenth = len;
	g_au8Reg = reg ;
	g_au8Buffer = data;

	g_u8EndFlag = 0;
	g_u8DataLen_m = 0;

	/* I2C function to read data from slave */
	I2Cx_Master_HandlerFn = (I2C_FUNC)I2Cx_MasterRx_multi;

//	printf("I2Cx_MasterRx_multi finish\r\n");
	
	I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA);
		
	#if defined (MASTER_I2C_TIMEOUT_ENABLE)
	I2C_EnableTimeout(MASTER_I2C, 0);
	#endif

	/* Wait I2C Rx Finish */
	while(g_u8EndFlag == 0);

	/* Make sure I2C0 STOP already */
	while(MASTER_I2C->CTL0 & I2C_CTL0_STO_Msk);
}

void I2Cx_Master_Init(void)
{    
    /* Open I2C module and set bus clock */
    I2C_Open(MASTER_I2C, 400000);
    
     /* Set I2C 4 Slave Addresses */            
    // I2C_SetSlaveAddr(MASTER_I2C, 0, 0x15, 0);   /* Slave Address : 0x15 */

	#if defined (MASTER_I2C_USE_IRQ)
	
	#if defined (MASTER_I2C_TIMEOUT_ENABLE)
//	I2C_EnableTimeout(MASTER_I2C ,0);
	#endif

    /* Enable I2C interrupt */
    I2C_EnableInt(MASTER_I2C);
    NVIC_EnableIRQ(MASTER_I2C_IRQn);
    #endif
}

void I2Cx_Master_example (uint8_t res)
{
	static uint32_t cnt = 0;	
	uint8_t u8RxData[16] = {0};
	uint8_t u8TxData[16] = {0};
	uint8_t u8CalData[16] = {0};
	
	static uint16_t DataCnt = 10;	

	uint8_t addr = 0;
	uint8_t reg = 0;
	uint32_t i = 0;	
	uint16_t len = 0;
	uint8_t crc = 0;
	uint8_t execute_time = 0;
	
	addr = 0x58;
	reg = 0x04;
	len = 4;	// not include reg length

	printf("I2Cx_Master_example start\r\n");

//	I2Cx_Master_Init();

	//clear data
	for(i = 0; i < 16; i++)
	{
		u8TxData[i] = 0;
	}

	switch(res)
	{
		case 0 :
			I2Cx_Master_GPIO_Recover();
			break;

		case 1 :
			printf("W)addr : 0x%2X, reg : 0x%2X \r\n",addr,reg);		
			//fill data
			u8TxData[0] = 0x12 ;
			u8TxData[1] = 0x34 ;	
			u8TxData[2] = DataCnt ;
			u8TxData[3] = 0x5A ;

			#if defined (MASTER_I2C_USE_IRQ)
			I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
			I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);		
			#endif
			
			printf("I2Cx_Write finish\r\n");
			printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);

			DataCnt++;

			break;

		case 2 :		
			//clear data
			for(i = 0; i < 16; i++)
			{
				u8RxData[i] = 0;
			}

			#if defined (MASTER_I2C_USE_IRQ)	
			I2Cx_Read_Multi_FromSlave(addr,reg,u8RxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
			I2C_ReadMultiBytesOneReg(MASTER_I2C, addr, reg, u8RxData, len);
			#endif			
			
			printf("\r\nI2Cx_Read finish\r\n");
			
			for(i = 0; i < 4 ; i++)
			{
				printf("0x%2X ,", u8RxData[i]);
		   	 	if ((i+1)%8 ==0)
		        {
		            printf("\r\n");
		        }		
			}

			printf("\r\n\r\n\r\n");
		
			break;

		case 3 :
			//calculate crc
			u8CalData[0] = reg ;	//flag
			u8CalData[1] = DataCnt;	//duty
			crc = CRC_Get(u8CalData , 2);
			
			//fill data
			u8TxData[0] = u8CalData[1] ;
			u8TxData[1] = crc;
			u8TxData[2] = 0x5A;
			u8TxData[3] = 0xA5;

			#if defined (MASTER_I2C_USE_IRQ)
			I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
			I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);		
			#endif
			
			printf("I2Cx_Write finish\r\n");
			printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);

			DataCnt++;
			
			break;

		case 4 :
			//calculate crc
			u8CalData[0] = reg ;	//flag
			u8CalData[1] = 0x0D;	//type
			u8CalData[2] = 0x1D;		
			crc = CRC_Get(u8CalData , 3);
			
			//fill data
			u8TxData[0] = u8CalData[1];
			u8TxData[1] = u8CalData[2];
			u8TxData[2] = crc;
			u8TxData[3] = 0x5A ;

			#if defined (MASTER_I2C_USE_IRQ)
			I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
			I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);		
			#endif
			
			printf("I2Cx_Write finish\r\n");
			printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);
			
			break;

		case 5 :
			//calculate crc
			u8CalData[0] = reg ;	//flag
			u8CalData[1] = 0x01;	//type
			u8CalData[2] = 0x1E;			
			crc = CRC_Get(u8CalData , 3);
			
			//fill data
			u8TxData[0] = u8CalData[1];
			u8TxData[1] = u8CalData[2];
			u8TxData[2] = crc;
			u8TxData[3] = 0x5A ;

			#if defined (MASTER_I2C_USE_IRQ)
			I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
//			I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);	
			I2C_WriteMultiBytes(MASTER_I2C , addr ,u8TxData , len); 		
			#endif
			
			printf("I2Cx_Write finish\r\n");
			printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);
			
			break;


		case 6 :
			//calculate crc
			u8CalData[0] = reg ;	//flag
			u8CalData[1] = 0x01;//type
			u8CalData[2] = 0x1F;				
			crc = CRC_Get(u8CalData , 3);
			
			//fill data
			u8TxData[0] = u8CalData[1];
			u8TxData[1] = u8CalData[2];
			u8TxData[2] = crc;
			u8TxData[3] = 0x5A ;

			#if defined (MASTER_I2C_USE_IRQ)
			I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
//			I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);	
			I2C_WriteMultiBytes(MASTER_I2C , addr ,u8TxData , len); 		
			#endif
			
			printf("I2Cx_Write finish\r\n");
			printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);
			
			break;

		case 7 :		
			//clear data
			for(i = 0; i < 16; i++)
			{
				u8RxData[i] = 0;
			}

			#if defined (MASTER_I2C_USE_IRQ)	
			I2Cx_Read_Multi_FromSlave(addr,reg,u8RxData,len);
			#elif defined (MASTER_I2C_USE_POLLING)
			I2C_ReadMultiBytesOneReg(MASTER_I2C, addr, reg, u8RxData, len);
			#endif			
			
			printf("\r\nI2Cx_Read finish\r\n");
			
			for(i = 0; i < 4 ; i++)
			{
				printf("0x%2X ,", u8RxData[i]);
		   	 	if ((i+1)%8 ==0)
		        {
		            printf("\r\n");
		        }		
			}

			printf("\r\n\r\n\r\n");
			break;

		case 8 : 
			execute_time = 0;
			do
			{			
				//fill data
				u8TxData[0] = 0x12 ;
				u8TxData[1] = 0x34 ;
				u8TxData[2] = DataCnt ;
				u8TxData[3] = 0x5A ;

				#if defined (MASTER_I2C_USE_IRQ)
				I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
				#elif defined (MASTER_I2C_USE_POLLING)
				I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);		
				#endif
				
				printf("I2Cx_Write finish\r\n");
				printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);
				
				DataCnt++;
				
				//clear data
				for(i = 0; i < 16; i++)
				{
					u8RxData[i] = 0;
				}

				#if defined (MASTER_I2C_USE_IRQ)	
				I2Cx_Read_Multi_FromSlave(addr,reg,u8RxData,len);
				#elif defined (MASTER_I2C_USE_POLLING)
				I2C_ReadMultiBytesOneReg(MASTER_I2C, addr, reg, u8RxData, 16);
				#endif			
				
				printf("\r\nI2Cx_Read finish\r\n");
				
				for(i = 0; i < 4 ; i++)
				{
					printf("0x%2X ,", u8RxData[i]);
					if ((i+1)%8 ==0)
					{
						printf("\r\n");
					}		
				}

				printf("\r\n\r\n\r\n");	
			} while (execute_time++ < 1);
				
			break;
			
		case 9 :
			execute_time = 0;
			do
			{
				//fill data	
				u8TxData[0] = 0x56 ;
				u8TxData[1] = 0x78 ;
				u8TxData[2] = DataCnt ;
				u8TxData[3] = 0x5A ;

				#if defined (MASTER_I2C_USE_IRQ)
				I2Cx_Write_Multi_ToSlave(addr,reg,u8TxData,len);
				#elif defined (MASTER_I2C_USE_POLLING)
				I2C_WriteMultiBytesOneReg(MASTER_I2C, addr, reg, u8TxData, len);		
				#endif
				
				// printf("I2Cx_Write finish\r\n");
				// printf("addr : 0x%2X, reg : 0x%2X , data (%2d) : \r\n",addr,reg,cnt);
				
				DataCnt++;				

				// printf("\r\n\r\n\r\n");	
			} while (execute_time++ < 5);
				
			break;
	}	
}

void I2Cx_Master_GPIO_Recover(void)
{
	uint8_t i = 0;
    SYS_UnlockReg();

    SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC4MFP_Msk)) |
                    (SYS_GPC_MFPL_PC5MFP_GPIO | SYS_GPC_MFPL_PC4MFP_GPIO);

    GPIO_SetMode(PC, BIT5, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT4, GPIO_MODE_OUTPUT);	

    SET_SCL;
    SET_SDA;
       
    for (i = 0 ; i < 9 ; i++)
    {
        SET_SCL;   //SET_SCL;
        CLK_SysTickDelay(10);
        RESET_SCL;  //RESET_SCL;
        CLK_SysTickDelay(10);      
    }

    // STOP
    RESET_SDA;  //RESET_SDA;
    CLK_SysTickDelay(10);
    SET_SCL;   //SET_SCL;
    CLK_SysTickDelay(10);
    SET_SDA;   //SET_SDA;
    CLK_SysTickDelay(10);

	g_u8EndFlag = 1;

	/* Reset and initiate I2C again*/
	I2C_Close(MASTER_I2C);

	SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC4MFP_Msk)) |
					(SYS_GPC_MFPL_PC5MFP_I2C1_SCL | SYS_GPC_MFPL_PC4MFP_I2C1_SDA);		
	/* Open I2C module and set bus clock */
	I2C_Open(MASTER_I2C, 100000);  

    SYS_LockReg();
	
	printf("%s done\r\n",__FUNCTION__);
}


