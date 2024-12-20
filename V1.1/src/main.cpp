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

#define AVERAGEFROM 100 // Max 60

#if MAXAQ > 118
#error "MAXAQ may nog be greater than 118"
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

uint8_t maxOutputVoltage; // Max output voltage (5-10V) based on the potentiometer, only determined at start of calibration

uint8_t getCalibCurrentValue(uint8_t i)
{
  return EEPROM_read(i + 1);
}
void setCalibCurrentValue(uint8_t i, uint8_t value)
{
  EEPROM_write(i + 1, value);
}

uint16_t readAvgADC(uint8_t adc)
{
  uint32_t sum = 0;
  for (int i = 0; i < AVERAGEFROM; i++)
  {
    sum += readADC(adc);
    delay(4);
  }
  return (uint32_t)(sum / AVERAGEFROM);
}

void setCalibrationValues()
{
  EEPROM_write(0, maxOutputVoltage);
}

void getCalibrationValues()
{
  maxOutputVoltage = EEPROM_read(0);
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
      write(PB, Q_LED_CALIBRATION, LOW);
      alarmState = false;
      wrongCount = 0;
    }

    bool buttonState = !(PINA & (1 << I_BUTTON));
    if (buttonState)
      nextState = CALIBRATIONSTATE;

    uint16_t analogSum = readAvgADC(AI_ANALOG);

    static uint8_t lastOutputVoltage = 0;
    uint8_t outputVoltage = (uint32_t)analogSum * maxOutputVoltage / 186;
    if (outputVoltage > maxOutputVoltage)
      outputVoltage = maxOutputVoltage;

    if (outputVoltage > lastOutputVoltage + 2 || outputVoltage < lastOutputVoltage - 2) // If the difference is larger than 2, use the calculated value
      outputVoltage = outputVoltage;
    else if (outputVoltage > lastOutputVoltage + 1) // If the difference is equal to 2, change the value by 1
      outputVoltage = lastOutputVoltage + 1;
    else if (outputVoltage < lastOutputVoltage - 1)
      outputVoltage = lastOutputVoltage - 1;
    // If the difference is only 1, don't change the output voltage

    writeAQ(blockVoltage ? 0 : outputVoltage);
    lastOutputVoltage = outputVoltage;

    // delay(100);

    // Determine if current is wrong
    bool wrong = false;
    uint16_t currentSum = readAvgADC(AI_CURRENT);
    if (currentSum >= 102)
      currentSum -= 102;
    else
      currentSum = 0;
    uint8_t faultPercentage = getFaultPercentage();
    uint16_t minCurrent = (uint32_t)getCalibCurrentValue(outputVoltage) * (100 - faultPercentage) / 100;
    uint16_t maxCurrent = (uint32_t)getCalibCurrentValue(outputVoltage) * (100 + faultPercentage) / 100;
    if (currentSum + 1 < minCurrent)
      wrong = true;
    if (maxCurrent < (currentSum - 1) && currentSum >= 1)
      wrong = true;
    if (!wrong && wrongCount)
      wrongCount--;
    else if (wrong)
      wrongCount++;

    if (!alarmState){
      alarmState = blockVoltage = wrongCount >= maxWrongCount;
      if(alarmState)
        alarmOnMils = millis();
    }
    else if (millis() - alarmOnMils >= 5000) // After alarm goes on, the 0-10V should be throughputted for hand mode
      blockVoltage = false;

    write(PA, Q_RELAY, !alarmState);
    write(PB, Q_ALARM, alarmState);
    write(PB, Q_LED_ALARM, alarmState || wrong);
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
      write(PB, Q_LED_ALARM, LOW);
      write(PB, Q_ALARM, LOW);
      // Calculate the maximum ouput voltage based on the potentiometer
      // maxOutputVoltage = MAXAQ / 2 + (uint32_t)(1023 - readADC(AI_POT)) * (MAXAQ / 2 + MAXAQ % 2) / 1023; // Dont know why but it outputs 8V when turning pot to 10V
      if(readADC(AI_POT) > 511) // 5V
        maxOutputVoltage = MAXAQ / 2 + MAXAQ % 2;
      else // 10V
        maxOutputVoltage = MAXAQ;
      outputVoltage = maxOutputVoltage;
      writeAQ(outputVoltage);
      lastIncrementMillis = mils;
    }

    if (mils - lastIncrementMillis >= 3000)
    {
      uint16_t currentSum = readAvgADC(AI_CURRENT);
      if (currentSum >= 102)
        currentSum -= 102;
      else
        currentSum = 0;

      setCalibCurrentValue(outputVoltage, currentSum);

      if (outputVoltage >= 1)
        outputVoltage -= 1;
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