#include <Arduino.h>
#include <SPI.h>

#define Q_LED_POWER PB0
#define Q_LED_ALARM PB5
#define Q_LED_CALIBRATION PB6
#define Q_ALARM PB3

#define I_BUTTON PA7
#define Q_RELAY PA6
#define CS_AQ PA3
#define SPI_SCK PB2
#define SPI_MOSI PB0
#define SPI_DO PB1

#define AI_VIN 0 // ADC0
#define AI_ANALOG 1
#define AI_SWITCH 2
#define AI_CURRENT 3
#define AI_DIP 4 // ADC4
#define AI_POT 7

#define MAXAQ 116 // MAX 118! //The value when the AQ outputs 10V

#define AVERAGEFROM 5

#if MAXAQ > 124
#error "MAXAQ may nog be greater than 124"
#endif

void write(int port, int pin, bool state)
{
  static byte porta = 0;
  static byte portb = 0;
  if (port == PA)
  {
    if (state)
      porta |= (1 << pin);
    else
      porta &= ~((byte)1 << pin);
    PORTA = porta;
  }
  else
  {
    if (state)
      portb |= (1 << pin);
    else
      portb &= ~((byte)1 << pin);
    PORTB = portb;
  }
}

bool read(int port, int pin)
{
  if (port == PA)
  {
    byte porta = PORTA;
    return porta & (1 << pin);
  }
  else
  {
    byte portb = PORTB;
    return portb & (1 << pin);
  }
}

void SPITransfer(byte data)
{
  for (int i = 0; i < 8; i++)
  {
    write(PB, SPI_SCK, false);
    write(PB, SPI_DO, data & (1 << (7 - i)));
    delayMicroseconds(1);
    write(PB, SPI_SCK, true);
    delayMicroseconds(1);
  }
}

// Output value 0..1023
uint16_t readADC(int ADCInput)
{
  // ADC0..10 available
  if (ADCInput > 10)
    return -1;
  // Select input
  ADMUX = ADCInput;
  // Start conversion
  ADCSRA = B11000000;
  // Wait for conversion finished
  while (B01000000 & ADCSRA)
    ;
  // Return result (first read ADCL!)
  return ADCL | (ADCH << 8);
}

uint8_t getFaultPercentage()
{
  uint16_t dipVal = readADC(AI_DIP);
  static uint8_t faultPercentage = 5;
  if (dipVal > 800)
    faultPercentage = 5;
  else if (dipVal > 200 && dipVal < 400)
    faultPercentage = 10;
  else if (dipVal > 440 && dipVal < 700)
    faultPercentage = 15;
  else if (dipVal < 150)
    faultPercentage = 20;
  return faultPercentage;
}

// Input range: 0..MAXAQ
void writeAQ(uint8_t value)
{
  write(PA, CS_AQ, LOW);
  SPITransfer(0);                             // start byte
  SPITransfer(value < MAXAQ ? value : MAXAQ); // max = 127
  write(PA, CS_AQ, HIGH);
}

void EEPROM_write(unsigned char ucAddress, unsigned char ucData)
{
  /* Wait for completion of previous write */
  while (EECR & (1 << EEPE))
    ;
  /* Set Programming mode */
  EECR = (0 << EEPM1) | (0 << EEPM0);
  /* Set up address and data registers */
  EEAR = ucAddress;
  EEDR = ucData;
  /* Write logical one to EEMPE */
  EECR |= (1 << EEMPE);
  /* Start eeprom write by setting EEPE */
  EECR |= (1 << EEPE);
}

unsigned char EEPROM_read(unsigned char ucAddress)
{
  /* Wait for completion of previous write */
  while (EECR & (1 << EEPE))
    ;
  /* Set up address register */
  EEAR = ucAddress;
  /* Start eeprom read by writing EERE */
  EECR |= (1 << EERE);
  /* Return data from data register */
  return EEDR;
}

uint8_t maxOutputVoltage;

uint16_t maxCurrent;
uint16_t getCalibCurrentValue(uint8_t i){
  return (uint32_t)EEPROM_read(i + 5)  * maxCurrent / 255;
}
void setCalibCurrentValue(uint8_t i, uint32_t value){
  EEPROM_write(i + 5, value * 255 / maxCurrent);
}

uint16_t maxVoltage;
uint16_t getCalibVoltageValue(uint8_t i){
  return (uint32_t)EEPROM_read(i + 65)  * maxVoltage / 255;
}
void setCalibVoltageValue(uint8_t i, uint32_t value){
  EEPROM_write(i + 65, value * 255 / maxVoltage);
}

void setCalibrationValues()
{
  uint16_t maxCurrent = 0, maxVoltage = 0;
  // Determine the maximum value
  for (int i = 0; i < maxOutputVoltage / 2 + 1; i++)
  {
    if (calibCurrentValues[i] > maxCurrent)
      maxCurrent = calibCurrentValues[i];
    if (calibVoltageValues[i] > maxVoltage)
      maxVoltage = calibVoltageValues[i];
  }

  // Save the maximum value
  EEPROM_write(0, maxCurrent >> 8);
  EEPROM_write(1, maxCurrent & 0x00FF);
  EEPROM_write(2, maxVoltage >> 8);
  EEPROM_write(3, maxVoltage & 0x00FF);
  EEPROM_write(4, maxOutputVoltage);

  // Save the values scaled from 0..MaxOutputVoltage to 0..255
  for (int i = 0; i < maxOutputVoltage / 2; i++)
  {
    EEPROM_write(i + 5, (uint32_t)calibCurrentValues[i] * 255 / maxCurrent);
    EEPROM_write(i + 65, (uint32_t)calibVoltageValues[i] * 255 / maxVoltage);
  }
}

void getCalibrationValues()
{
  // Inverted setCalibrationValues

  // Get the maximum values
  uint16_t maxCurrent = EEPROM_read(0) << 8 | EEPROM_read(1);
  uint16_t maxVoltage = EEPROM_read(2) << 8 | EEPROM_read(3);
  uint8_t maxOutputVoltageCalib = EEPROM_read(4);

  // Get the values scaled from 0..255 to 0..MaxOutputVoltageCalib
  for (int i = 0; i < maxOutputVoltageCalib / 2; i++)
  {
    calibCurrentValues[i] = (uint32_t)EEPROM_read(i + 5)  * maxCurrent / 255;
    calibVoltageValues[i] = (uint32_t)EEPROM_read(i + 65)  * maxVoltage / 255;
  }
}

void setup()
{
  cli();                 // Disable interrupts
  CLKPR = (1 << CLKPCE); // Prescaler enable
  CLKPR = 0x00;          // Clock division factor
  sei();                 // Enable interrupts

  // Config port A+B, 0 = input, 1 = output
  DDRA = (0 << I_BUTTON) | (1 << Q_RELAY) | (1 << CS_AQ);
  DDRB = (1 << SPI_DO) | (1 << Q_ALARM) | (1 << Q_LED_ALARM) | (1 << Q_LED_CALIBRATION) | (1 << Q_LED_POWER) | (1 << SPI_SCK) | (1 << SPI_MOSI);

  write(PA, I_BUTTON, 1); // set pull-up
  write(PA, CS_AQ, HIGH);
  write(PB, SPI_SCK, HIGH);
  write(PB, Q_LED_POWER, HIGH);

  // Enable ADC (ADC Control and Status Register A)
  ADCSRA = B10000000;

  writeAQ(0);

  getCalibrationValues();
}

enum State
{
  DEFAULTSTATE,
  CALIBRATIONSTATE
};

void loop()
{
  static State lastState = DEFAULTSTATE;
  static State state = DEFAULTSTATE;
  static State nextState = DEFAULTSTATE;

  // Calculate the maximum ouput voltage based on the potentiometer
  maxOutputVoltage = MAXAQ / 2 + (1023 - readADC(AI_POT)) * (MAXAQ / 2 + MAXAQ % 2) / 1023;

  switch (state)
  {
  case DEFAULTSTATE:
  {
    if (lastState != DEFAULTSTATE)
    {
      write(PB, Q_LED_CALIBRATION, LOW);
    }

    bool switchState = !(PINA & (1 << I_BUTTON));
    if (switchState)
      nextState = CALIBRATIONSTATE;

    break;
  }
  case CALIBRATIONSTATE:
  {
    static uint8_t outputVoltage;
    static unsigned long lastIncrementMillis;
    unsigned long mils = millis();

    if (lastState != CALIBRATIONSTATE)
    {
      write(PB, Q_LED_CALIBRATION, HIGH);
      write(PA, Q_RELAY, HIGH);
      write(PA, Q_LED_ALARM, LOW);
      outputVoltage = 0;
      writeAQ(outputVoltage);
      lastIncrementMillis = mils;
    }

    if (mils - lastIncrementMillis >= 2000)
    {
      int currentSum = 0;
      int voltageSum = 0;
      for (int i = 0; i < AVERAGEFROM; i++)
      {
        currentSum += readADC(AI_CURRENT);
        voltageSum += readADC(AI_SWITCH);
        delay(10);
      }
      setCalibCurrentValue[outputVoltage / 2] = currentSum / AVERAGEFROM;
      calibVoltageValues[outputVoltage / 2] = voltageSum / AVERAGEFROM;
      if (maxOutputVoltage > outputVoltage)
        outputVoltage += 2;
      else
      {
        setCalibrationValues();
        nextState = DEFAULTSTATE;
      }
      writeAQ(outputVoltage);
      lastIncrementMillis = mils;
    }

    break;
  }
  default:
    break;
  }

  lastState = state;
  state = nextState;
}