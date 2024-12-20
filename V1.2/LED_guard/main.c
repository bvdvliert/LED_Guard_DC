/**********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO
 * THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2020-2022 Renesas Electronics Corporation. All rights reserved.
 *********************************************************************************************************************/
/***********************************************************************/
/*                                                                     */
/*  FILE        :Main.c or Main.cpp                                    */
/*  DATE        :                                                      */
/*  DESCRIPTION :Main Program                                          */
/*  CPU TYPE    :                                                      */
/*                                                                     */
/*  									                               */
/*                                                                     */
/***********************************************************************/

#include "src/smc_gen/general/r_smc_entry.h"

#define ADC_MAX (4095)

void main(void);

enum State
{
	DEFAULTSTATE,
	CALIBRATIONSTATE
};

void main(void)
{
	R_Systeminit();
	R_Config_ADC_Set_OperationOn();

	PIN_WRITE(Q_LED1) = 1;
	PIN_WRITE(Q_LED2) = 0;
	PIN_WRITE(Q_LED3) = 0;

	while (1)
	{
		static enum State lastState = DEFAULTSTATE;
		static enum State state = DEFAULTSTATE;
		static enum State nextState = DEFAULTSTATE;

		switch (state)
		{
		case DEFAULTSTATE:
			const uint8_t maxWrongCount = 10;
			static uint8_t wrongCount = 0;
			static bool alarmState = false;
			static bool blockVoltage = false;
			static uint32_t alarmOnMils;

			if (lastState != DEFAULTSTATE)
			{
				PIN_WRITE(Q_LED2) = 0;
				alarmState = false;
				wrongCount = 0;
			}

			bool buttonState = !PIN_READ(I_BUTTON);
			if (buttonState)
				nextState = CALIBRATIONSTATE;

			R_Config_ADC_Set_ADChannel(ANI0_POT);
			R_Config_ADC_Start();
			while (ADIF == 0);
			static uint16_t potVal = 0;
			R_Config_ADC_Get_Result_12bit(&potVal);
			PIN_WRITE(Q_LED3) = (potVal > (ADC_MAX / 2));

			break;
		case CALIBRATIONSTATE:
			static uint8_t outputVoltage;
			static unsigned long lastIncrementMillis;
			// unsigned long mils = millis();

			if (lastState != CALIBRATIONSTATE)
			{
				PIN_WRITE(Q_LED2) = 1;
				PIN_WRITE(Q_RELAY) = 1;
				PIN_WRITE(Q_LED3) = 0;
				PIN_WRITE(Q_ALARM) = 0;

				// Calculate the maximum ouput voltage based on the potentiometer
				// maxOutputVoltage = MAXAQ / 2 + (uint32_t)(1023 - readADC(AI_POT)) * (MAXAQ / 2 + MAXAQ % 2) / 1023; // Dont know why but it outputs 8V when turning pot to 10V
				nextState = DEFAULTSTATE;
			}

			break;
		default:
			state = DEFAULTSTATE;
			break;
		}

		lastState = state;
		state = nextState;
	}
}
