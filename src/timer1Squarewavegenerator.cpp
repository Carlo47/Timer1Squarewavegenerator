/**
 * Program      timer1squarewavegenerator.cpp
 * Author       2021-06-07 Charles Geiser (https://www.dodeka.ch)
 * 
 * Purpose      Shows how to realize a wide range squarewave generator with Timer1.
 *              Takes the frequency or the period from serial input
 *              frequency: 1 .. 8'000'000 Hz
 *              period:    1 .. 8'000'000 us
 *              and outputs a squarewave on pin 9 or 10
 * 
 *              Frequency and period can be entered numerically as integer values
 *              but by entering values for output control register and prescaler directly 
 *              fractional frequencies as low as 0.12 HZ are possible
 * 
 * Output       500.00 Hz  /  2000.00 us
 *   example    PRESC: 1
 *              OCRA1: 0x3E7F  /  15999
 * 
 * Caveat       Depending on the computed values for prescaler and comparison value, the
 *              resulting output values differ from the desired values. The effective 
 *              values for frequency and period as well as the set values for prescaler
 *              and ocr register are shown on the serial monitor
 * 
 *              To enable snprintf() for Arduino you have to use special build flags (see platformio.ino): 
 *              build_flags = -Wl,-u,vfprintf -lprintf_flt -lm 
 * 
 * Board        Arduino uno
 *
 * Wiring       Oscilloscope on pin 9 or 10
 *
 * Remarks      Uses Timer1 in CTC mode (clear timer on compare)
 *              prescaler:       3 least significant bits of TCCR1B = 0b00000xxx
 *                               stand for a divider of 1, 8, 64, 256, 1024 for fo
 *              compare value:   OCR1A (output compare register) contains values 0x0000 .. 0xFFFF
 * 
 *              fo               8'000'000 (half of fcpu)
 *              f                desired frequency in Hz
 *              T                desired period in seconds
 *              Tus              desired period in microseconds
 *              ocr              content of 16-bit register OCR1A 
 *  
 * Formulas     f = fo / ((ocr + 1) * pre),  fo = 8'000'000 Hz and pre = 1, 8, 64, 256, 1024
 *              T = (ocr+1) * pre / fo
 * 
 *              ocr = fo / f / pre - 1
 *              ocr = T * fo / pre - 1  
 *                  = Tus * 8 / pre - 1
 *              
 *              If the value of ocr is greater than 0xFFFF, the next higher prescaler is taken
 * 
 *              The tables below show the resulting frequencies and periods for the possible 
 *              prescalers and the ocr values 0x0000 and 0xFFFF. 
 * 
 *                                              Resulting frequencies [Hz]
 *                            -------------------------------------------------------------- 
 *              pre =         1            8            64           256          1024
 *                            --------------------------------------------------------------
 *              ocr = 0x0000  8000000	     1000000	    125000	     31250	      7812.5
 *              ocr = 0xFFFF  122.0703125	 15.25878906	1.907348633  0.476837158	0.11920929
 *
 *	                                            Resulting periods [us]
 *                            --------------------------------------------------------------				
 *              pre =         1            8            64           256          1024
 *                            --------------------------------------------------------------
 *              ocr = 0x0000	0.125        1            8            32           128
 *              ocr = 0xFFFF	8192         65536        524288       2097152      8388608
 * 
 * Reference  https://arduino-projekte.webnode.at/meine-projekte/servosteuerung/servotest/variante2/
 *            https://www.youtube.com/watch?v=wk_pxRhVNgA
 *            https://wolles-elektronikkiste.de/timer-und-pwm-teil-2-16-bit-timer1
 *            http://www.gammon.com.au/timers
 */
#include <Arduino.h>

// Clear the current line with a carriage return, then printing 80 blanks 
// followed by another carriage return to position the cursor on line beginning
#define CLR_LINE    "\r                                                                                \r"

enum class INPUT_MODE { FREQUENCY, PERIOD };

// Definition of a menuitem
typedef struct { const char key; const char *txt; void (&action)(); } MenuItem;
 
void toggleInputMode();
void enterValue();
void setPrescaler();
void setOCR1A();
void toggleOutputPin();
void toggleHeartbeat();
void showSettings();
void showMenu(); 

// Menu definition. Each menuitem is composed of a key, a text and an action
MenuItem menu[] = 
{
  { 'f', "[f] Toggle input mode frequency <--> period",   toggleInputMode },
  { 'e', "[e] Enter a value 1 .. 8000000 (freq or per)",  enterValue },
  { 'p', "[p] Enter prescaler 1=1 2=8 3=64 4=256 5=1024", setPrescaler },
  { 'r', "[r] Enter OCR1A 0 .. 65535",                    setOCR1A },
  { 'o', "[o] Toggle output pin 9 <--> 10",               toggleOutputPin },
  { 'h', "[h] Toggle heartbeat on <--> off",              toggleHeartbeat },
  { 's', "[s] Show settings",                             showSettings },
  { 'S', "[S] Show menu",                                 showMenu },
};
constexpr uint8_t nbrMenuItems = sizeof(menu) / sizeof(menu[0]);

bool heartbeatEnabled = true;
uint8_t        pinOut = 9;                     // default output pin, can be changed to 10 on serial monitor
uint32_t     freq_per = 1000;                  // holds frequency or period value 
INPUT_MODE       mode = INPUT_MODE::FREQUENCY; // input mode defaults to frequency, can be changed to period on serial monitor

/**
 * Set frequency between 1 .. 8'000'000 Hz
 */
void setFrequency(uint32_t freq, uint8_t pin)
{
  const uint32_t fo = 8000000;
  uint32_t pre = 1;

  TCCR1A = 0;  // clear the register
  if (pin ==  9) TCCR1A = 1 << COM1A0;  // set output pin
  if (pin == 10) TCCR1A = 1 << COM1B0;

  TCCR1B = 0b00001001; // Prescaler = 001 = 1
  //             ^--- 
  //             | |
  //             | prescaler bits
  //             WGM12 bit for CTC mode 

  // Why these values were chosen for the decisions
  // can be seen from the tables in the header
  if (freq < 123) 
  {
    TCCR1B = 0b00001010; // Prescaler: 010 = 8
    pre = 8;
  }

  if (freq < 16)
  {
    TCCR1B = 0b00001011; // Prescaler: 011 = 64
    pre = 64;
  }

  if (freq < 2)
  {
    TCCR1B = 0b00001100; // Prescaler: 100 = 256
    pre = 256;
  }
  
  if (freq < 1)
  {
    TCCR1B = 0b00001101; // Prescaler: 101 = 1024
  }
  OCR1A = (uint16_t) (round( (double)fo / (double)pre / (double)freq - 1.0 ));
  TIMSK1 = 0;             // Timer 1 interrupt mask register
}

/**
 * Set period between 1 .. 8'000'000 us
 **/
void setPeriod(uint32_t period, uint8_t pin)
{
  uint32_t pre = 8;

  TCCR1A = 0;
  if (pin ==  9) TCCR1A = 1 << COM1A0;  // set output pin
  if (pin == 10) TCCR1A = 1 << COM1B0;

  TCCR1B = 0b00001010; // Prescaler: 010 = 8, resulting step 1 us

  // Why these values were chosen for the decisions
  // can be seen from the tables in the header
  if (period > 65536)
  {
    TCCR1B = 0b00001011; // Prescaler: 011 = 64, resulting step 8 us
    pre = 64;
  }

  if (period > 524288)
  {
    TCCR1B = 0b00001100; // Prescaler: 100 = 256, resulting step 32 us
    pre = 256;
  }  

  if (period > 2097152)
  {
    TCCR1B = 0b00001101; // Prescaler: 101 = 1024, resulting step 128 us
    pre = 1024;
  } 

  if (period == 0)
  {
    TCCR1B = 0b00001001; // Prescaler: 001 = 1, resulting step 0.125 us
    pre = 1;
  }

  uint32_t ocr_long = period * 8 / pre - 1;
  OCR1A = (uint16_t)ocr_long;
  TIMSK1 = 0;             // Timer 1 interrupt mask register
}

/**
 * Calculate frequency from register values
 */
double getFrequencyFromRegisters()
{
  int      pre[] = { 0, 1, 8, 64, 256, 1024};  // possible prescaler values
  uint16_t preBits = TCCR1B & 0b00000111;      // get bits of prescaler
  double   frequency = 8000000.0 / (uint32_t(OCR1A) + 1) / pre[preBits]; 
  return frequency;
}

/**
 * Calculate period from register values
 */
double getPeriodFromRegisters()
{
  int      pre[] = { 0, 1, 8, 64, 256, 1024};  // possible prescaler values
  uint16_t preBits = TCCR1B & 0b00000111;      // get bits of prescaler
  double   period = ((double)OCR1A + 1) * pre[preBits] / 8.0;
  return period;  
}

/**
 * Get prescaler and content of OCR1A and compute resulting frequency and period.
 * Show both values and also prescaler and OCR1A in hex and decimal. 
 */
void printRegisterSettings()
{
int      pre[] = { 0, 1, 8, 64, 256, 1024};  // possible prescaler values
uint16_t preBits = TCCR1B & 0b00000111;      // get bits of prescaler
double   frequency;
double   period;
char     buf[64];

frequency = 8000000.0 / (uint32_t(OCR1A) + 1) / pre[preBits];
period = ((double)OCR1A + 1) * pre[preBits] / 8.0;
// to use snprintf() with floats use the build_flags = -Wl,-u,vfprintf -lprintf_flt -lm
snprintf(buf, sizeof(buf), "%.2f Hz / %.2f us, PRESC: %d, OCR1A: 0x%04X / %u ", frequency, period, pre[preBits], OCR1A, OCR1A);
Serial.print(buf);
}

/**
 * Switch input mode between frequency and period
 */
void toggleInputMode()
{
  if (mode == INPUT_MODE::FREQUENCY)
  {
    mode = INPUT_MODE::PERIOD;
    Serial.print("Input mode set to PERIOD ");
  } 
  else
  {
    mode = INPUT_MODE::FREQUENCY;
    Serial.print("Input mode set to FREQUENCY ");
  } 
}

/**
 * Enter a value, either for the frequency or 
 * the period, depending on the input mode
 */
void enterValue()
{
  uint32_t value = 0;

  delay(2000);
  while (Serial.available())
  {
    value = Serial.parseInt();
  }

  if (value < 1 || value > 8000000)
  {
    Serial.print("Value out of range, allowed: 1 .. 8'000'000 (Hz or us)");
    return;
  }
  if (mode == INPUT_MODE::FREQUENCY)
  {
    setFrequency(value, pinOut);
  }
  else
  {
    setPeriod(value, pinOut);
  }
  printRegisterSettings();
}

/**
 * Set the prescaler
 */
void setPrescaler()
{
  int32_t preBits;

  delay(2000);
  while (Serial.available())
  {
    preBits = Serial.parseInt();
  }

  if (preBits < 1 || preBits > 5)
  {
    Serial.println("Value out of range, allowed: 1 .. 5 ");
    return;
  }
  
  TCCR1B &= 0b11111000; // clear the prescaler bits
  TCCR1B |= (uint8_t)preBits;    // set the new value
  printRegisterSettings();
}

/**
 * Set the output control register OCR1A
 */
void setOCR1A()
{
  int32_t value;

  delay(2000);
  while (Serial.available())
  {
    value = Serial.parseInt();
  }

  if (value < 0 || value > 0xffff)
  {
    Serial.println("Value out of range, allowed: 0 .. 65535 ");
    return;
  }
  
  OCR1A = (uint16_t)value;
  printRegisterSettings();
}

/**
 * Switch output signal from pin 9 to pin 10 and vice versa
 */
void toggleOutputPin()
{
  TCCR1A = 0;

  if (pinOut == 9)
  {
    pinOut = 10;
    TCCR1A = 1 << COM1B0;
    Serial.print("Output pin set to 10");
  }
  else
  {
    pinOut = 9;
    TCCR1A = 1 << COM1A0;
    Serial.print("Output pin set to 9");
  }
}

/**
 * Turn on or off flashing led
 */
void toggleHeartbeat()
{
  heartbeatEnabled = !heartbeatEnabled;
  if (heartbeatEnabled)
    Serial.print("Heartbeat on ");
  else
    Serial.print("Heartbeat off ");
}

/**
 * Show frequency [Hz], period [us], prescaler
 * and output control register OCR1A
 */
void showSettings()
{
  printRegisterSettings();
}

/**
 * Display menu on monitor
 */
void showMenu()
{
  // title is packed into a raw string
  Serial.print(
  R"TITLE(
------------------------------
 Timer 1 Square Wave Generator 
    0.12  .. 8'000'000 Hz
------------------------------
)TITLE");

  for (int i = 0; i < nbrMenuItems; i++)
  {
    Serial.println(menu[i].txt);
  }
  Serial.print("\nPress a key: ");
}

/**
 * Execute the action assigned to the key
 */
void doMenu()
{
  char key = Serial.read();
  Serial.print(CLR_LINE);
  for (int i = 0; i < nbrMenuItems; i++)
  {
    if (key == menu[i].key)
    {
      menu[i].action();
      break;
    }
  } 
}

/**
 * Flash the led on pin with period and pulse width
 */
void heartbeat(uint8_t pin, uint32_t period, uint32_t pulseWidth)
{
  digitalWrite(pin, millis() % period < pulseWidth ? HIGH : LOW);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  setFrequency(freq_per, pinOut); // default frequency is 1000 Hz on pin 9
  showMenu();
}

void loop()
{
  // handle the menu
  if (Serial.available()) doMenu();
  if (heartbeatEnabled)   heartbeat(LED_BUILTIN, 1000, 20); 
}
