/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products.
* No other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING THIS SOFTWARE, WHETHER EXPRESS, IMPLIED
* OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT.  ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY
* LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE FOR ANY DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR
* ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability 
* of this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2021, 2024 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name        : Pin.h
* Version          : 1.0.0
* Device(s)        : R7F100GGFxFB
* Description      : This file implements SMC pin code generation.
***********************************************************************************************************************/

#ifndef PIN_H
#define PIN_H

/***********************************************************************************************************************
Macro definitions (Register bit)
***********************************************************************************************************************/

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/* User's guide for symbolic name.
 * The generated symbolic names can be used in the user application as follows: 
 *
 * Example: Toggle LED1 at Pin P54.
 *  There are 2 ways to toggle LED1
 *   1) Using symbolic name macro
 *    Assuming the symbolic name for P54 is "LED1", the generated macro definition will be:
 *         #define LED1    5,4
 *
 *    To use this macro definition to toggle the LED1, call the symbolic name APIs:
 *         PIN_WRITE(LED1) = ~PIN_READ(LED1)
 *
 *   2) Not using symbolic name macro
 *    Call the symbolic name APIs directly
 *         PIN_WRITE(5,4) = ~PIN_READ(5,4)
 */

/* Symbolic name */
#define Q_ALARM        1,0
#define ANI7_VIN        2,7
#define ANI6_AI        2,6
#define ANI5_SWITCH        2,5
#define ANI4_CURRENT        2,4
#define ANO0_AQ        2,2
#define ANI1_DIP        2,1
#define ANI0_POT        2,0
#define Q_RELAY        1,7
#define Q_RLY_AQ        1,6
#define Q_LED3        1,5
#define Q_LED2        1,4
#define Q_LED1        1,3
#define I_BUTTON        12,4

/* Pin write helper */
#define PIN_WRITE_HELPER(x,y)                    ((P##x##_bit.no##y))
/* Pin read helper */
#define PIN_READ_HELPER(x,y)                     ((P##x##_bit.no##y))

/* Pin write API */
#define PIN_WRITE(...)                           (PIN_WRITE_HELPER(__VA_ARGS__))
/* Pin read API */
#define PIN_READ(...)                            (PIN_READ_HELPER(__VA_ARGS__))

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Global functions
***********************************************************************************************************************/
void R_Pins_Create(void);
/* Start user code for function. Do not edit comment generated here */
#define ANI0_POT        _00_AD_INPUT_CHANNEL_0
#define ANI1_DIP        _01_AD_INPUT_CHANNEL_1
#define ANI4_CURRENT    _04_AD_INPUT_CHANNEL_4
#define ANI5_SWITCH     _05_AD_INPUT_CHANNEL_5
#define ANI6_AI         _06_AD_INPUT_CHANNEL_6
#define ANI7_VIN        _07_AD_INPUT_CHANNEL_7
/* End user code. Do not edit comment generated here */
#endif
