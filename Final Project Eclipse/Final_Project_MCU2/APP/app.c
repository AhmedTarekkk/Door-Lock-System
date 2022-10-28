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
uint8 UART_String[20];
uint8 PasswordState;
uint8 Password[PASSWORD_SIZE];
uint8 g_ticks = 0;
uint8 g_exitMotorFlag = 0;
uint8 g_MotorUnlocking = 0;
uint8 g_exitAlarmFlag = 0;
Timer1_ConfigType TIMER1_Configuration = {0,TIMER1_OCR1A,TIMER1_FCPU_1024,COMPARE};

/*******************************************************************************
*                      		Functions Definitions	             	           *
*******************************************************************************/

/*******************************************************************************
* Function Name:		APP_isPasswordSet
* Description:			Function to check if the password is already saved in EEPORM
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_isPasswordSet()
{
	/* To check if password is set in the EEPROM or not */
	EEPROM_readByte(Password_Is_Set_Address, &PasswordState);
	/* send the password state to MCU1 to handle the different cases */
	UART_sendByte(PasswordState);
}

/*******************************************************************************
* Function Name:		APP_readPassword
* Description:			Function to read the password saved in EEPROM into Password[] variable
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_readPassword()
{
	for(uint8 i = 0 ; i < PASSWORD_SIZE ; i++ )
	{
		EEPROM_readByte(Password_Address+i, (Password+i) );
	}
}

/*******************************************************************************
* Function Name:		APP_updatePassword
* Description:			Function to change the password
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_updatePassword()
{
	for(uint8 k = 0 ; k < PASSWORD_SIZE ; k++)
	{
		Password[k] = UART_receiveByte(); /* Getting the new password from MCU1 */
		EEPROM_writeByte(Password_Address+k, Password[k]); /* Write it in the EEPROM */
	}
	EEPROM_writeByte(Password_Is_Set_Address, PasswordSET); /* Update password state to be set*/
}

/*******************************************************************************
* Function Name:		APP_checkPassword
* Description:			Function to get the password from MCU1 and check if it = to the one saved in EEPROM
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_checkPassword()
{
	uint8 checkPassword[PASSWORD_SIZE]; /* Variable to save the password from the keypad in MCU1 */
	APP_readPassword(); /* Update the Password variable to be = to the password in the EEPROM */
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
* Function Name:		APP_alarm
* Description:			Function to turn on alarm and enter ERROR state for 60 seconds
* Parameters (in):    	None
* Parameters (out):   	None
* Return value:      	void
********************************************************************************/
void APP_alarm()
{
	g_ticks = 0;
	g_exitAlarmFlag = 0;
	/* initialize the timer module with the desired ISR */
	TIMER1_COMP_setCallBack(TIMER1_ALARM_ISR);
	TIMER1_init(&TIMER1_Configuration);
	BUZZER_on(); /* Turn on the alarm */
	while(g_exitAlarmFlag == 0){} /* wait until the timer counts 60 seconds */
	BUZZER_off(); /* Turn off the alarm */
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
	g_ticks = 0;
	/* initialize the timer module with the desired ISR */
	TIMER1_COMP_setCallBack(TIMER1_MOTOR_15SEC_ISR);
	TIMER1_init(&TIMER1_Configuration);
	DcMotor_Rotate(	DcMotor_CW, 100); /* opening the door */
	g_exitMotorFlag = 0;
	while(g_exitMotorFlag == 0){}
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
