/******************************************************************************
*  File name:		mc1.c
*  Author:			Oct 25, 2022
*  Author:			Ahmed Tarek
*******************************************************************************/

/*******************************************************************************
*                        		Inclusions                                     *
*******************************************************************************/
#include "APP/app.h"

/*******************************************************************************
*                        		Configurations                                 *
*******************************************************************************/
UART_ConfigType UART_Configuration = {9600,'#',UART_1_STOP_BIT,UART_8_BITS,UART_DISABLED_PARTIY,POLLING};
TWI_ConfigType TWI_Configuration = {1,400}; /* Slave Address = 1 , Baud rate = 400 Kbps */

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

	APP_isPasswordSet(); /* To check if password is set in the EEPROM or not for first time entring the program */

	while(1)
	{
		/* waiting MCU1 to send the message */
		uint8 MSG = UART_receiveByte();
		switch(MSG)
		{
		/* In case MCU1 wants to set the password or update it */
		case MSG_UpdatePassword:
			APP_updatePassword();
			break;
		/* In case MCU1 wants to enter ERROR state and turn the alarm on */
		case MSG_TurnOnAlarm:
			APP_alarm(); /* call the function that is responsible of turning alarm on for 60 seconds */
			break;
		/* In case MCU1 wants to enter open the door */
		case MSG_Motor:
			APP_door();/* call the function that is responsible of opening the door */
			break;
		/* In case MCU1 wants to know the password saved in EEPROM */
		case MSG_checkPassword:
			APP_checkPassword();
			break;
		}
	}
}
