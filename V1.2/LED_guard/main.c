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
#include "src/smc_gen/r_rfd_rl78_common/r_rfd_rl78_common_if.h"
#include "src/smc_gen/r_rfd_rl78_dataflash/r_rfd_rl78_data_flash_if.h"

#define ADC_MAX (4095)

#define DF_ADDRESS (0xF1000uL)
#define L_MCLK_FREQ_1MHz (1000000uL)
#define L_MCLK_ROUNDUP_VALUE (999999uL)

#define DAC_MAX (231) // 10.03 V

void main(void);

enum State
{
	DEFAULTSTATE,
	CALIBRATIONSTATE
};

struct calibrationValue
{
	uint16_t AI;
	uint16_t current;
	uint16_t voltage;
};

struct calibrationValue __near calibrationValues[DAC_MAX + 1];

void initDataFlash(void)
{
	uint32_t l_mclk_freq = R_BSP_GetFclkFreqHz();
	l_mclk_freq = (l_mclk_freq + L_MCLK_ROUNDUP_VALUE) / L_MCLK_FREQ_1MHz;
	R_RFD_Init((uint8_t)l_mclk_freq);
}

void waitSeqEnd(void)
{
	while (R_RFD_ENUM_RET_STS_BUSY == R_RFD_CheckCFDFSeqEndStep1())
		;
	while (R_RFD_ENUM_RET_STS_BUSY == R_RFD_CheckCFDFSeqEndStep2())
		;
	R_RFD_ClearSeqRegister();
}

e_rfd_ret_t writeDataFlash(uint32_t i_u32_start_addr,
						   uint16_t i_u16_write_data_length,
						   uint8_t __near *inp_u08_write_data)
{

	R_RFD_SetDataFlashAccessMode(R_RFD_ENUM_DF_ACCESS_ENABLE);

	e_rfd_ret_t l_e_rfd_ret_status = R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_DATA_PROGRAMMING);
	if (R_RFD_ENUM_RET_STS_OK != l_e_rfd_ret_status) // An error occurred
		return l_e_rfd_ret_status;

	R_RFD_EraseDataFlashReq(0);
	waitSeqEnd();

	for (uint16_t l_u16_count = 0u; l_u16_count < i_u16_write_data_length; l_u16_count++)
	{
		R_RFD_WriteDataFlashReq(i_u32_start_addr + l_u16_count, &inp_u08_write_data[l_u16_count]);
		waitSeqEnd();
	}

	l_e_rfd_ret_status = R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE);
	if (R_RFD_ENUM_RET_STS_OK != l_e_rfd_ret_status) // An error occurred
		return l_e_rfd_ret_status;

	R_RFD_SetDataFlashAccessMode(R_RFD_ENUM_DF_ACCESS_DISABLE);

	return R_RFD_ENUM_RET_STS_OK;
}

e_rfd_ret_t readDataFlash(uint32_t i_u32_start_addr,
						  uint16_t i_u16_read_data_length,
						  uint8_t __near *outp_u08_read_data)
{

	R_RFD_SetDataFlashAccessMode(R_RFD_ENUM_DF_ACCESS_ENABLE);

	for (uint16_t l_u16_count = 0u; l_u16_count < i_u16_read_data_length; l_u16_count++)
	{
		outp_u08_read_data[l_u16_count] = (*(uint8_t __far *)(i_u32_start_addr + l_u16_count));
	}

	R_RFD_SetDataFlashAccessMode(R_RFD_ENUM_DF_ACCESS_DISABLE);

	return R_RFD_ENUM_RET_STS_OK;
}

void delay(uint16_t clkticks)
{
	for (int i = 0; i < clkticks; i++)
		for (int j = 0; j < 1000; j++)
			;
}

void writeCalibrationValues(void)
{
	writeDataFlash(DF_ADDRESS, sizeof(calibrationValues), (uint8_t *)&calibrationValues);
}

void readCalibrationValues(void)
{
	readDataFlash(DF_ADDRESS, sizeof(calibrationValues), (uint8_t *)&calibrationValues);
}

uint16_t readADC(e_ad_channel_t channel)
{
	R_Config_ADC_Set_ADChannel(channel);
	R_Config_ADC_Start();
	while (ADIF == 0)
		;
	ADIF = 0; // Clear ADC interrupt flag
	uint16_t adcVal;
	R_Config_ADC_Get_Result_12bit(&adcVal);
	return adcVal;
}

long map(int64_t x, int64_t in_min, int64_t in_max, int64_t out_min, int64_t out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

uint8_t buf[2000];
void main(void)
{
	R_Systeminit();
	R_Config_ADC_Set_OperationOn();
	R_Config_DAC0_Create();
	R_Config_DAC0_Start();
	R_Config_DAC0_Set_ConversionValue(0);

	initDataFlash();

	for (int i = 0; i < 1500; i++)
		buf[i] = i + 1;

	writeDataFlash(DF_ADDRESS, 1500, buf);

	for (int i = 0; i < 1500; i++)
		buf[i] = 0;

	readDataFlash(DF_ADDRESS, 1500, buf);

	readCalibrationValues();

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
		{
			delay(100);
			const uint8_t maxWrongCount = 100;
			static uint8_t wrongCount = 0;
			static bool alarmState = false;

			if (lastState != DEFAULTSTATE)
			{
				PIN_WRITE(Q_LED2) = 0;
				PIN_WRITE(Q_RLY_AQ) = 0;
				R_Config_DAC0_Set_ConversionValue(0);
				alarmState = false;
				wrongCount = 0;
			}

			bool buttonState = !PIN_READ(I_BUTTON);
			if (buttonState)
				nextState = CALIBRATIONSTATE;

			int ai = readADC(ANI6_AI);
			int current = readADC(ANI4_CURRENT) - 410;
			int voltage = readADC(ANI7_VIN);
			struct calibrationValue belowVal = calibrationValues[DAC_MAX];
			struct calibrationValue aboveVal = calibrationValues[0];

			for (int i = 0; i <= DAC_MAX; i++)
				if (calibrationValues[i].AI > ai)
				{
					aboveVal = calibrationValues[i];
					break;
				}

			for (int j = DAC_MAX; j >= 0; j--)
				if (calibrationValues[j].AI < ai)
				{
					belowVal = calibrationValues[j];
					break;
				}

			int expectedCurrent = map(ai, belowVal.AI, aboveVal.AI, belowVal.current, aboveVal.current) - 410;
			// int expectedVoltage = map(ai, belowVal.AI, aboveVal.AI, belowVal.voltage, aboveVal.voltage);

			bool wrong = false;
			if (current < expectedCurrent - 10 || current > expectedCurrent + 10)
				if (current < 0.9 * expectedCurrent || current > 1.1 * expectedCurrent)
					wrong = true;

			if (!wrong && wrongCount > 0)
				wrongCount--;
			else if (wrong)
				wrongCount++;

			if (wrongCount > maxWrongCount)
				alarmState = true;

			PIN_WRITE(Q_LED3) = alarmState;
			PIN_WRITE(Q_ALARM) = alarmState;
			PIN_WRITE(Q_RELAY) = !alarmState;
			PIN_WRITE(Q_RLY_AQ) = alarmState; // use the 0V supplied by LED Guard

			break;
		}
		case CALIBRATIONSTATE:
		{
			static uint8_t outputVoltage;

			if (lastState != CALIBRATIONSTATE)
			{
				PIN_WRITE(Q_LED2) = 1;
				PIN_WRITE(Q_RELAY) = 1;
				PIN_WRITE(Q_LED3) = 0;
				PIN_WRITE(Q_ALARM) = 0;
				PIN_WRITE(Q_RLY_AQ) = 1;

				// Calculate the maximum ouput voltage based on the potentiometer
				// maxOutputVoltage = MAXAQ / 2 + (uint32_t)(1023 - readADC(AI_POT)) * (MAXAQ / 2 + MAXAQ % 2) / 1023; // Dont know why but it outputs 8V when turning pot to 10V

				outputVoltage = 0;
			}

			R_Config_DAC0_Set_ConversionValue(outputVoltage);
			delay(100);
			const int measavgcnt = 10;
			uint32_t aisum = 0;
			uint32_t currentsum = 0;
			uint32_t voltagesum = 0;
			for (int i = 0; i < measavgcnt; i++)
			{
				currentsum += readADC(ANI4_CURRENT);
				aisum += readADC(ANI6_AI);
				voltagesum += readADC(ANI7_VIN);
			}

			struct calibrationValue val;
			val.AI = aisum / measavgcnt;
			val.current = currentsum / measavgcnt;
			val.voltage = voltagesum / measavgcnt;

			calibrationValues[outputVoltage] = val;

			if (outputVoltage < DAC_MAX)
				outputVoltage += 1;
			else
			{
				writeCalibrationValues();
				readCalibrationValues();
				nextState = DEFAULTSTATE;
			}

			break;
		}
		default:
			state = DEFAULTSTATE;
			break;
		}

		lastState = state;
		state = nextState;
	}
}
