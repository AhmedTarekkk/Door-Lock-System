/******************************************************************************
*  File name:		app.c
*  Author:			Oct 25, 2022
*  Author:			Ahmed Tarek
*******************************************************************************/

/*******************************************************************************
*                        		Inclusions                                     *
*******************************************************************************/
#include "app.h"

/*******************************************************************************
*                           Global Variables                                  *
*******************************************************************************/
uint8 Password[PASSWORD_SIZE]; /* Variable to store the password first time and send it to MCU2 */
uint8 Password2[PASSWORD_SIZE]; /* Variable to store the password and send it to MCU2 to get checked */
uint8 PasswordMatchFlag; /* Flag to indicate if the two passwords match or not */
uint8 Lives = ALLOWED_TRIES; /* Number of tries u get to try to insert the right password */
uint8 g_ticks = 0; /* Variable to store number of seconds we get from the timer */
uint8 g_exitMotorFlag = 0; /* Flag to exit APP_dooPorcessing Function */
uint8 g_MotorUnlocking = 0; /* To decide to unlock or lock the door in the ISR*/
uint8 g_exitAlarmFlag = 0; /* Flag to exit ERROR state in case 3 password wrong in row */

Timer1_ConfigType TIMER1_Configuration = {0,TIMER1_OCR1A,TIMER1_FCPU_1024,COMPARE};

/*******************************************************************************
*                      		Functions Definitions	             	           *
*******************************************************************************/

/*******************************************************************************
* Function Name:		APP_setPassword
* Description:			Function to set the password in case no one set it before
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_setPassword()
{
	LCD_clearScreen();
	LCD_displayString("Set New Password");
	_delay_ms(1000);
	LCD_clearScreen();
	UART_sendByte(MSG_UpdatePassword); /* Inform MCU2 that it will receive new password to set it in the eeprom */
	do
	{
		PasswordMatchFlag = TRUE ;
		LCD_displayStringRowColumn(0, 0, "Plz Enter Pass");
		LCD_moveCursor(1, 0);
		for(uint8 i = 0 ; i < PASSWORD_SIZE ; i++) /* get the password from the user and save it into Password[] array */
		{
			uint8 key = KEYPAD_getPressedKey();
			if(key <= 9 && key >= 0)
			{
				Password[i] = key;
				LCD_displayCharacter('*');
			}
			else /* in case the user pressed a key that is not allowed */
			{
				i--; /* don't count this key and go back again */
			}
			_delay_ms(KEYPAD_BUTTON_DELAY); /* wait for a certain delay before getting another input */
		}
		while(KEYPAD_getPressedKey() != ENTER_KEY){} /* wait tell the user press Enter key on the keypad */


		LCD_clearScreen();
		LCD_displayStringRowColumn(0,0,"Plz Re-Enter the");
		LCD_displayStringRowColumn(1, 0, "Same Pass: ");
		for(uint8 i = 0 ; i < PASSWORD_SIZE ; i++) /* get the password again from the user and save it into Password2[] array */
		{
			uint8 key = KEYPAD_getPressedKey();
			if(key <= 9 && key >= 0)
			{
				Password2[i] = key;
				LCD_displayCharacter('*');
			}
			else
			{
				i--;
			}
			_delay_ms(KEYPAD_BUTTON_DELAY);
		}
		while(KEYPAD_getPressedKey() != ENTER_KEY){}


		for(uint8 j = 0 ; j < PASSWORD_SIZE	; j++) /* Compare between the two passwords to see if they match or not */
		{
			if(Password[j] != Password2[j])
			{
				PasswordMatchFlag = 0;
				break;
			}
		}
		LCD_clearScreen();
		if(PasswordMatchFlag == TRUE) /* if they match then change the password */
		{
			LCD_displayStringRowColumn(0, 4, "Matched");
			LCD_displayStringRowColumn(1, 0, "Password Updated");
			for(uint8 k = 0 ; k < PASSWORD_SIZE ; k++)
			{
				UART_sendByte(Password[k]); /* as MCU2 already on MSG_UpdatePassword state and it is waiting for the new password to be sent */
				_delay_ms(50);
			}
		}
		else /* if they are not display UnMatched on the LCD */
		{
			LCD_displayStringRowColumn(0, 0, "UnMatched");
		}
		_delay_ms(1000);
		LCD_clearScreen();

	}while(PasswordMatchFlag == FALSE); /* Restart the whole process until the user enters two match passwords */
}

/*******************************************************************************
* Function Name:		APP_comparePassWithEEPROM
* Description:			Function to get input password from user and compare it with the one saved in EEPORM
* Parameters (in):    	None
* Parameters (out):   	MSG_Matched or MSG_UnMatched
* Return value:      	uint8
********************************************************************************/
uint8 APP_comparePassWithEEPROM()
{
	Lives = ALLOWED_TRIES; /* Number of Tries Allowed */
	do
	{
		PasswordMatchFlag = TRUE;
		LCD_clearScreen();
		LCD_displayStringRowColumn(0, 0, "Plz Enter old");
		LCD_displayStringRowColumn(1, 0, "Password: ");
		for(uint8 i = 0 ; i < PASSWORD_SIZE ; i++) /* Getting the password from the user */
		{
			uint8 key = KEYPAD_getPressedKey();
			if(key <= 9 && key >= 0)
			{
				Password2[i] = key;
				LCD_displayCharacter('*');
			}
			else /* in case the user pressed a key that is not allowed */
			{
				i--; /* don't count this key and go back again */
			}
			_delay_ms(KEYPAD_BUTTON_DELAY); /* wait for certain time before pressing another button */
		}
		while(KEYPAD_getPressedKey() != ENTER_KEY){} /* wait the user to hit enter "ON/C" */

		UART_sendByte(MSG_checkPassword);  /* Telling MCU2 that MCU1 want to check if the password match the one in the EEPROM */

		for(uint8 j = 0 ; j < PASSWORD_SIZE	; j++) /* send the claimed password */
		{
			UART_sendByte(Password2[j]);
			while(UART_receiveByte() != MC_Ready);
		}
		LCD_clearScreen();
		if(UART_receiveByte() == MSG_Matched) /* in case they are match print Matched on LCD and return MSG_Matched */
		{
			LCD_displayStringRowColumn(0, 4, "Matched");
			_delay_ms(1000);
			return MSG_Matched; /* to exit the whole function */
		}
		else /* in case they are not matched, decrease number of tries and try again if tries > 0 */
		{
			Lives--; /* decrease number of tries by 1 if the user missed */
			LCD_displayStringRowColumn(0, 3, "UnMatched");
			LCD_displayStringRowColumn(1, 0, "Tries left = ");
			LCD_intgerToString(Lives);
			_delay_ms(1000);
		}
	}while((Lives > 0)); /* wait until the user uses all number of tries available */
	return MSG_UnMatched; /* in case the user used all his tries and didn't get the password right at any one of them */
}

/*******************************************************************************
* Function Name:		APP_alarm
* Description:			Function to turn on alarm and enter ERROR state for 60 seconds
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_alarm()
{
	LCD_clearScreen();
	LCD_displayStringRowColumn(0, 3, "ERROR !!!");
	UART_sendByte(MSG_TurnOnAlarm); /* Telling MCU2 to turn on the buzzer */
	g_ticks = 0;
	g_exitAlarmFlag = 0;
	TIMER1_init(&TIMER1_Configuration); /* setting the time to display ERROR message on LCD for 60 seconds */
	TIMER1_COMP_setCallBack(TIMER1_ALARM_ISR);
	while(g_exitAlarmFlag == 0){} /* wait until the timer count the required time and handle different cases then exit */
}

/*******************************************************************************
* Function Name:		APP_changePassword
* Description:			Function to check password and change it if the user input it right
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_changePassword()
{
	uint8 PasswordsCompare = APP_comparePassWithEEPROM() ;
	if(PasswordsCompare == MSG_Matched)
	{
		APP_setPassword(); /* set the new password */
	}
	if(PasswordsCompare == MSG_UnMatched) /* if the user used didn't get the password right in all of his tries */
	{
		APP_alarm(); /* Turn on the alarm */
	}

}

/*******************************************************************************
* Function Name:		APP_door
* Description:			Function to control locking and unlocking the door
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_door()
{
	uint8 PasswordsCompare = APP_comparePassWithEEPROM() ;
	if(PasswordsCompare == MSG_Matched)
	{
		UART_sendByte(MSG_Motor);
		g_ticks = 0;
		TIMER1_COMP_setCallBack(TIMER1_MOTOR_15SEC_ISR); /* unlock the door takes 15 seconds */
		TIMER1_init(&TIMER1_Configuration);
		LCD_displayStringRowColumn(0, 3, "Unlocking");
		LCD_displayStringRowColumn(1, 3, "The Door");
		g_exitMotorFlag = 0;
		while(g_exitMotorFlag == 0){} /* wait until the timer count the required time and handle different cases then exit */
	}
	if(PasswordsCompare == MSG_UnMatched) /* if the user used didn't get the password right in all of his tries */
	{
		APP_alarm(); /* Turn on the alarm */
	}
}

/*******************************************************************************
*                      		INTERRUPT SERVICE ROUTINE	           	           *
*******************************************************************************/

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
	if(g_ticks == 15) /* wait until number of seconds = 15 */
	{
		g_ticks = 0; /* clear the number of seconds so we can use it in other functions*/
		LCD_clearScreen();
		if(g_MotorUnlocking == 0) /* in case it is the first time to come here then we want to unlock the door */
		{
			LCD_displayStringRowColumn(0, 0, "Door Is Locked");
			/* Waiting MC1 to be Ready as LCD is slow at writing and Turning Motor On or OFF is fast so there will
			 * a delay that we can solve by waiting LCD to write then turn the motor on or off */
			UART_sendByte(MC_Ready);
			g_MotorUnlocking++; /* go to the other branch next time */
			TIMER1_COMP_setCallBack(TIMER1_MOTOR_3SEC_ISR); /* to wait another 3 seconds then lock the door again */
		}
		else if(g_MotorUnlocking == 1) /* in case it is the second time to come here then we handled every case for the door and we want to exit*/
		{
			g_MotorUnlocking = 0;
			g_exitMotorFlag = 1; /* to exit the function */
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
		g_ticks = 0; /* clear the number of seconds */
		LCD_clearScreen();
		LCD_displayStringRowColumn(0, 3, "Locking");
		LCD_displayStringRowColumn(1, 3, "The Door");
		/* Waiting MC1 to be Ready as LCD is slow at writing and Turning Motor On or OFF is fast so there will
		 * a delay that we can solve by waiting LCD to write then turn the motor on or off */
		UART_sendByte(MC_Ready);
		TIMER1_COMP_setCallBack(TIMER1_MOTOR_15SEC_ISR); /* so we can count another 15 seconds then stop the motor */
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
	if(g_ticks == 60) /* wait until number of seconds = 60 */
	{
		g_ticks = 0;
		g_exitAlarmFlag = 1; /* exit ERROR state */
		TIMER1_deInit(); /* stop the timer */
	}
}
