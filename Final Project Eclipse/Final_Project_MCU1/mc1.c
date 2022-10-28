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

/*******************************************************************************
*           					Main Function                                 *
*******************************************************************************/
int main(void)
{
	/* Initialize different modules */
	KEYPAD_init();
	LCD_init();
	UART_init(&UART_Configuration);
	sei(); /* Enable Global interrupt */
	/* Telling MCU2 that MCU1 did the initialization stage */
	UART_sendByte(MC_Ready);
	/* To check if password is set in the EEPROM or not */
	uint8 PasswordState = UART_receiveByte();

	if(PasswordState != PasswordSET)
	{
		/* If password not set we got to set password function */
		APP_setPassword();
	}

	while(1)
	{
		LCD_clearScreen();
		LCD_displayStringRowColumn(0, 0, "+ : Open Door");
		LCD_displayStringRowColumn(1, 0, "- : Change Pass");
		uint8 menuKey; /* variable to decide  to open the door or change the password from the menu */
		do{
			menuKey = KEYPAD_getPressedKey();
		}while(menuKey != '-' && menuKey != '+'); /* wait until we get '+' or '-' */
		LCD_clearScreen();

		switch(menuKey)
		{
		case '+':
			APP_door(); /* Open the door function */
			break;

		case '-':
			APP_changePassword(); /* Change password function */
			break;
		}
	}
}
