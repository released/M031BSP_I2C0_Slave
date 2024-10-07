/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include "NuMicro.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/
/*_____ D E F I N I T I O N S ______________________________________________*/
#define MASTER_I2C_USE_IRQ
//#define MASTER_I2C_USE_POLLING

// #define DEBUG_LOG_MASTER_LV1
// #define DEBUG_LOG_SLAVE_LV1
// #define DEBUG_LOG_SLAVE_LV2
// #define DEBUG_LOG_SLAVE_STATEMACHINE_LV1

// #define MASTER_I2C_TIMEOUT_ENABLE
#define SLAVE_I2C_TIMEOUT_ENABLE

//#define MASTER_I2C						  		(I2C0)
//#define MASTER_I2C_IRQn						  	(I2C0_IRQn)
//#define I2Cx_Master_IRQHandler					(I2C0_IRQHandler)

#define MASTER_I2C						  		(I2C1)
#define MASTER_I2C_IRQn						  	(I2C1_IRQn)
#define I2Cx_Master_IRQHandler					(I2C1_IRQHandler)

#define MASTER_START_TRANSMIT			  		(0x08)
#define MASTER_REPEAT_START               		(0x10)
#define MASTER_TRANSMIT_ADDRESS_ACK       		(0x18)
#define MASTER_TRANSMIT_ADDRESS_NACK      		(0x20)
#define MASTER_TRANSMIT_DATA_ACK          		(0x28)
#define MASTER_TRANSMIT_DATA_NACK         		(0x30)
#define MASTER_ARBITRATION_LOST           		(0x38)
#define MASTER_RECEIVE_ADDRESS_ACK        		(0x40)
#define MASTER_RECEIVE_ADDRESS_NACK       		(0x48)
#define MASTER_RECEIVE_DATA_ACK           		(0x50)
#define MASTER_RECEIVE_DATA_NACK          		(0x58)
#define BUS_ERROR                         		(0x00)

#define SLAVE_I2C						  		(I2C0)
#define SLAVE_I2C_IRQn						  	(I2C0_IRQn)
#define I2Cx_Slave_IRQHandler					(I2C0_IRQHandler)

#define SLAVE_TRANSMIT_REPEAT_START_OR_STOP		(0xA0)
#define SLAVE_TRANSMIT_ADDRESS_ACK 				(0xA8)
#define SLAVE_TRANSMIT_DATA_ACK					(0xB8)
#define SLAVE_TRANSMIT_DATA_NACK             	(0xC0)
#define SLAVE_TRANSMIT_LAST_DATA_ACK         	(0xC8)
#define SLAVE_RECEIVE_ADDRESS_ACK            	(0x60)
#define SLAVE_RECEIVE_ARBITRATION_LOST       	(0x68)
#define SLAVE_RECEIVE_DATA_ACK               	(0x80)
#define SLAVE_RECEIVE_DATA_NACK              	(0x88)
#define GC_MODE_ADDRESS_ACK                		(0x70)
#define GC_MODE_ARBITRATION_LOST             	(0x78)
#define GC_MODE_DATA_ACK                   		(0x90)
#define GC_MODE_DATA_NACK                  		(0x98)
#define ADDRESS_TRANSMIT_ARBITRATION_LOST    	(0xB0)

/*_____ M A C R O S ________________________________________________________*/
/*_____ F U N C T I O N S __________________________________________________*/
void I2Cx_Master_Init(void);
void I2Cx_Master_example (uint8_t res);
void I2Cx_Master_GPIO_Recover(void);

void I2Cx_Slave_Init(void);
void I2Cx_Slave_example (void);


