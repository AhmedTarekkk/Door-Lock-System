/******************************************************************************
*  File name:		mc1.c
*  Author:			Oct 25, 2022
*  Author:			Ahmed Tarek
*******************************************************************************/

/*******************************************************************************
*                        		Inclusions                                     *
*******************************************************************************/
#include "MCAL/UART/uart.h"
#include "MCAL/TWI/twi.h"
#include "MCAL/TIMER1/timer1.h"
#include "HAL/BUZZER/buzzer.h"
#include "HAL/EXT_EEPORM/eeprom.h"
#include "HAL/MOTOR/motor.h"
#include "avr/interrupt.h"

/*******************************************************************************
*                        		Definitions                                    *
*******************************************************************************/
#define TIMER1_OCR1A				8000 	/* as we need TCNT1 to be 8000 so we can get interrupt every 1 sec */
#define PASSWORD_SIZE				5 		/* password array size */
#define Password_Address			0x350 	/* Password Location in the EEPROM */
#define	Password_Is_Set_Address		0x320	 /* Password flag Location in the EEPROM */
#define PasswordSET					0xC2 	/* To indicate whether the password is set or not */

/*******************************************************************************
*                        		UATR MESSAGES                                  *
*******************************************************************************/
#define MC_Ready					0xFC /* Message to indicate if the MCU is ready or not */
#define MSG_UpdatePassword			0x99 /* Message From MCU1 to MCU2 to inform it that it will send new password and replace the one you have with it */
#define MSG_TurnOnAlarm				0x88 /* Message From MCU1 to MCU2 to inform it the user entered the password wrong for 3 times, turn on the alarm */
#define MSG_checkPassword			0x77 /* Message From MCU1 to MCU2 to inform it that it will send password from keypad to get checked */
#define MSG_Motor					0x20 /* Message From MCU1 to MCU2 to inform it the user entered the password right, open the door */
#define MSG_Matched					0xF0 /* Message From MCU2 to MCU1 to inform it if the passwords match or not */
#define MSG_UnMatched				0x0F /* Message From MCU2 to MCU1 to inform it if the passwords match or not */

/*******************************************************************************
*                        		Configurations                                 *
*******************************************************************************/
UART_ConfigType UART_Configuration = {9600,'#',UART_1_STOP_BIT,UART_8_BITS,UART_DISABLED_PARTIY,POLLING};
Timer1_ConfigType TIMER1_Configuration = {0,TIMER1_OCR1A,TIMER1_FCPU_1024,COMPARE};
TWI_ConfigType TWI_Configuration = {1,400}; /* Slave Address = 1 , Baud rate = 400 Kbps */

/*******************************************************************************
*                           Global Variables                                  *
*******************************************************************************/
uint8 UART_String[20];
uint8 PasswordState;
uint8 Password[PASSWORD_SIZE];
uint8 g_ticks = 0;
uint8 g_exitMotorFlag = 0;
uint8 g_MotorUnlocking = 0;
uint8 g_exitAlarmFlag = 0;

/*******************************************************************************
*                      		Functions Prototypes	             	           *
*******************************************************************************/
void App_updatePassword();
void App_checkPassword();
void App_readPassword();
void TIMER1_MOTOR_3SEC_ISR();
void TIMER1_MOTOR_15SEC_ISR();
void TIMER1_ALARM_ISR();

/*******************************************************************************
*           					Main Function                                 *
*******************************************************************************/
int main(void)
{
	/* Initialize different modules */
	TWI_init(&TWI_Configuration);
	BUZZER_init();
	DcMotor_Init();
	UART_init(&UART_Configuration);
	sei();
	/* waiting MCU1 to be ready */
	while( UART_receiveByte() != MC_Ready){}
	/* To check if password is set in the EEPROM or not */
	EEPROM_readByte(Password_Is_Set_Address, &PasswordState);
	/* send the password state to MCU1 to handle the different cases */
	UART_sendByte(PasswordState);

	while(1)
	{
		/* waiting MCU1 to send the message */
		uint8 MSG = UART_receiveByte();
		switch(MSG)
		{
		/* In case MCU1 wants to set the password or update it */
		case MSG_UpdatePassword:
			App_updatePassword();
			break;
		/* In case MCU1 wants to enter ERROR state and turn the alarm on */
		case MSG_TurnOnAlarm:
			g_ticks = 0;
			g_exitAlarmFlag = 0;
			/* initialize the timer module with the desired ISR */
			TIMER1_COMP_setCallBack(TIMER1_ALARM_ISR);
			TIMER1_init(&TIMER1_Configuration);
			BUZZER_on(); /* Turn on the alarm */
			while(g_exitAlarmFlag == 0){} /* wait until the timer counts 60 seconds */
			BUZZER_off(); /* Turn off the alarm */
			break;
		/* In case MCU1 wants to enter open the door */
		case MSG_Motor:
			g_ticks = 0;
			/* initialize the timer module with the desired ISR */
			TIMER1_COMP_setCallBack(TIMER1_MOTOR_15SEC_ISR);
			TIMER1_init(&TIMER1_Configuration);
			DcMotor_Rotate(	DcMotor_CW, 100); /* opening the door */
			g_exitMotorFlag = 0;
			while(g_exitMotorFlag == 0){}
			break;
		/* In case MCU1 wants to know the password saved in EEPROM */
		case MSG_checkPassword:
			App_checkPassword();
			break;
		}
	}
}

/*******************************************************************************
*                      		Functions Definitions	             	           *
*******************************************************************************/

/*******************************************************************************
* Function Name:		App_readPassword
* Description:			Function to read the password saved in EEPROM into Password[] variable
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void App_readPassword()
{
	for(uint8 i = 0 ; i < PASSWORD_SIZE ; i++ )
	{
		EEPROM_readByte(Password_Address+i, (Password+i) );
	}
}

/*******************************************************************************
* Function Name:		App_updatePassword
* Description:			Function to change the password
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void App_updatePassword()
{
	for(uint8 k = 0 ; k < PASSWORD_SIZE ; k++)
	{
		Password[k] = UART_receiveByte(); /* Getting the new password from MCU1 */
		EEPROM_writeByte(Password_Address+k, Password[k]); /* Write it in the EEPROM */
	}
	EEPROM_writeByte(Password_Is_Set_Address, PasswordSET); /* Update password state to be set*/
}

/*******************************************************************************
* Function Name:		App_checkPassword
* Description:			Function to get the password from MCU1 and check if it = to the one saved in EEPROM
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void App_checkPassword()
{
	uint8 checkPassword[PASSWORD_SIZE]; /* Variable to save the password from the keypad in MCU1 */
	App_readPassword(); /* Update the Password variable to be = to the password in the EEPROM */
	for(uint8 k = 0 ; k < PASSWORD_SIZE ; k++) /* Receiving the password from MCU1 */
	{
		checkPassword[k] = UART_receiveByte();
		UART_sendByte(MC_Ready);
	}
	for(uint8 j = 0 ; j < PASSWORD_SIZE ; j++) /* check if they are matched or not */
	{
		if(Password[j] != checkPassword[j])
		{
			UART_sendByte(MSG_UnMatched);
			return;
		}
	}
	UART_sendByte(MSG_Matched);
}

/*******************************************************************************
* Function Name:		TIMER1_MOTOR_15SEC_ISR
* Description:			ISR function for the timer in case we want to count 15 seconds and lock or unlock the door
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void TIMER1_MOTOR_15SEC_ISR(void)
{
	g_ticks++;
	if(g_ticks == 15)
	{
		g_ticks = 0;
		if(g_MotorUnlocking == 0) /* wait until number of seconds = 15 */
		{
			/* Waiting MC1 to be Ready as LCD is slow at writing and Turning Motor On or OFF is fast so there will
			 * a delay that we can solve by waiting LCD to write then turn the motor on or off */
			UART_receiveByte();
			DcMotor_Rotate(	DcMotor_STOP, 100); /* the door now is unlocked */
			g_MotorUnlocking++;
			TIMER1_COMP_setCallBack(TIMER1_MOTOR_3SEC_ISR); /* to wait another 3 seconds then lock it again */
		}
		else if(g_MotorUnlocking == 1) /* in case it is the second time to come here then we handled every case for the door and we want to exit*/
		{
			g_MotorUnlocking = 0;
			g_exitMotorFlag = 1; /* to exit the function */
			DcMotor_Rotate(	DcMotor_STOP, 100); /* Stop the motor */
			TIMER1_deInit(); /* stop the timer */
		}
	}
}

/*******************************************************************************
* Function Name:		TIMER1_MOTOR_3SEC_ISR
* Description:			ISR function for the timer in case we want to count 3 seconds then lock the door
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void TIMER1_MOTOR_3SEC_ISR(void)
{
	g_ticks++;
	if(g_ticks == 3) /* wait until number of seconds = 3 */
	{
		g_ticks = 0;
		/* Waiting MC1 to be Ready as LCD is slow at writing and Turning Motor On or OFF is fast so there will
		 * a delay that we can solve by waiting LCD to write then turn the motor on or off */
		UART_receiveByte();
		DcMotor_Rotate(	DcMotor_CCW, 100); /* Lock the door again */
		TIMER1_COMP_setCallBack(TIMER1_MOTOR_15SEC_ISR); /* to count 15 seconds then stop the motor */
	}
}

/*******************************************************************************
* Function Name:		TIMER1_ALARM_ISR
* Description:			ISR function for the timer in case we want to count 60 seconds then turn off the alarm
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void TIMER1_ALARM_ISR()
{
	g_ticks++;
	if(g_ticks == 60)  /* wait until number of seconds = 60 */
	{
		g_ticks = 0;
		g_exitAlarmFlag = 1; /* exit ERROR state and turn off the buzzer */
		TIMER1_deInit(); /* stop the timer */
	}
}
