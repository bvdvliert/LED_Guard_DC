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

void main(void);

enum State
{
	DEFAULTSTATE,
	CALIBRATIONSTATE
};

void initDataFlash()
{
	uint32_t l_mclk_freq = R_BSP_GetFclkFreqHz();
	l_mclk_freq = (l_mclk_freq + L_MCLK_ROUNDUP_VALUE) / L_MCLK_FREQ_1MHz;
	R_RFD_Init((uint8_t)l_mclk_freq);
}

void waitSeqEnd()
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

uint8_t buf[1000];
void main(void)
{
	R_Systeminit();
	R_Config_ADC_Set_OperationOn();

	initDataFlash();
	
	for(int i = 0; i < 40; i++)
			buf[i] = i+1;

	// writeDataFlash(DF_ADDRESS, 100, buf);

	for(int i = 0; i < 40; i++)
			buf[i] = 0;

	readDataFlash(DF_ADDRESS, 10, buf);

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

			PIN_WRITE(Q_LED3) = (readADC(ANI0_POT) > (ADC_MAX / 2));

			break;
		}
		case CALIBRATIONSTATE:
		{
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
		}
		default:
			state = DEFAULTSTATE;
			break;
		}

		lastState = state;
		state = nextState;
	}
}
