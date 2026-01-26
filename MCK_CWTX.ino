#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// TFT pins SPI1 plus:
//      Vcc 3.3V             --->  ILI9340 Pin# 1
//      GND                  --->          Pin# 2
const int TFT_CS   = D10; // --->          Pin# 3
//      TFT_RST      3V3; // --->          Pin# 4
const int TFT_DC   = D9;  // --->          Pin# 5
//      TFT_MOSI //  D11     --->          Pin# 6
//      TFT_SCK  //  D13     --->          Pin# 7
//      LED backlight  3V3   --->          Pin# 8
//      TFT_MISO       N/C   --->          Pin# 9

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC); // Use hardware SPI 

const int IN_TX = LOW;
const int IN_RX = HIGH;
int TXStatus = IN_RX;
int LastTXStatus = IN_RX;
  
// DDS Pins
const int DDSRST = D5;
const int DATA  = D6;  
const int FQ_UD  = D3;
const int CLOCK = D2;

// CW Keying Pins
// output is to the shaping circuit
const int CWKeyOutput = A6;x`
// input is from CW Key jack
const int CWKeyInput = A7;

// TX Line
const int TXLine = A3;

// rotary encoder pins
const int pin_A = A0; 
const int pin_B = A1; 
const int BTN   = A2;   

// these values are used by the rotary encoder routines
uint8_t prevNextCode = 0;
uint16_t store=0;

// Sidetone pin to speaker
const int SideTone = A4;

// TX RX Relay
const int TXRXRelay = A5;

// constants for the upper and lower limits of the VFO
const uint32_t VFOUpperLimit = 7200000;
const uint32_t VFOLowerLimit = 7000000;

int cnt_step = 0;
int cnt_step_old = 0;
uint32_t TargetFrequency = 7028000; // this is the frequency on power up
uint8_t TargetPhase = 0;
uint32_t TargetFrequency_old;

const int CWPitch = 600;

// DDS Clock should be 125,000,000 but mine is a little low
// this is your best bet at calibration
const uint32_t DDSClock = 125000000; //124986400;

int BtnPress = LOW;
int LastBtnPress = LOW;
unsigned long lastDebounceTime = 0;  // the last time the encoder button was toggled
unsigned long debounceDelay = 180;   // the debounce time; fiddle if the output flickers

uint16_t DDSStep = 100;

void setup() 
{
  // define pins for the TFT display
  // we only need to worry about non SPI pins
  pinMode(TFT_DC, OUTPUT);  // TFT pins (plus SPI1)
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_DC, LOW); // internal pull-down
  digitalWrite(TFT_CS, LOW);
  
  // DDS pins are handled by the Init routine
  
  // encoder pins
  pinMode (pin_A,INPUT_PULLUP);
  pinMode (pin_B,INPUT_PULLUP);   
  pinMode (BTN,INPUT_PULLUP);   

  // CW key line output to shaping circuit
  pinMode (CWKeyOutput, OUTPUT);
  digitalWrite (CWKeyOutput, HIGH);

  // external CW Key Jack
  pinMode (CWKeyInput, INPUT_PULLUP);

  // TX RX Relay
  pinMode (TXRXRelay, OUTPUT);
  digitalWrite (TXRXRelay, LOW);

  // we take this low to tell the software we are in TX
  pinMode (TXLine, INPUT_PULLUP);

  // define the PWM CW Sidetone
  pinMode (SideTone, OUTPUT);
  
  // Init the DDS
  DDSInit();

  // setup and initialise the screen
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 110, 360, 120, ILI9341_RED);
  tft.setCursor(95, 10);
  tft.setTextColor(ILI9341_RED);    
  tft.setTextSize(5);
  tft.println("G0MGX");
  tft.setTextColor(ILI9341_GREEN);  
  tft.setCursor(70, 60);
  tft.setTextSize(4);
  tft.println("40M CWTX");
  
  // initialise to 7.028 MHz
  TargetFrequency = 7028000;
  TargetFrequency_old = TargetFrequency;

  SetFrequency(TargetFrequency, TargetPhase);
  DisplayFreq(TargetFrequency);

  DisplayTXRX(TXStatus);
}

void DisplayTXRX(int TXStatus)
{
  tft.fillRect(0, 180, 360, 70, ILI9341_RED);
  tft.setTextSize(3);  
  tft.fillRect(10, 180, 50, 35, ILI9341_WHITE);
  tft.setCursor(18,187);
  tft.setTextColor(ILI9341_GREEN);
  
  if (TXStatus == IN_RX)
  {
    tft.setTextColor(ILI9341_GREEN);
    tft.print("Rx");
  }
  else
  {
    tft.setTextColor(ILI9341_RED);
    tft.print("Tx");  
  }
}

void loop(void) 
{
  static int CWKeyState = HIGH;
  static int LastCWKeyState = HIGH;
  static int CWKeyReading = HIGH;
  
  static unsigned long LastDebounceTime = 0;
  const unsigned long DebounceDelay = 20; //ms

  // check for button press
  StepSelect();
  if (cnt_step != cnt_step_old) 
  {
    // if its changed we need to update the old
    // value and update the display
    cnt_step_old = cnt_step;
    DisplayFreq(TargetFrequency);
  }

  // check for tuning change
  rotary_enc();
  if (TargetFrequency != TargetFrequency_old) {
    TargetFrequency_old = TargetFrequency;
    // set DDS frequency if its changed
    // and update the display
    SetFrequency(TargetFrequency - CWPitch, TargetPhase);
    DisplayFreq(TargetFrequency);
  }

  TXStatus = digitalRead(TXLine);

  // read the input CW line
  CWKeyReading = digitalRead(CWKeyInput);

  // if it has changed start the debounce timer
  if ((CWKeyReading != LastCWKeyState) && (TXStatus == IN_TX))
  {
    LastDebounceTime = millis();
  }

  // if we have passed the debounce delay time
  if (((millis() - LastDebounceTime) > DebounceDelay) && (TXStatus == IN_TX))
  {
    // and its different to the last time we set it
    if (CWKeyReading != CWKeyState)
    {
      // set the current state to be the latest reading
      CWKeyState = CWKeyReading;
      // and set the output in accordance with the current state
      if (CWKeyState == LOW)
      {
        // low is key down so we need a low output
        digitalWrite (CWKeyOutput, LOW);
        tone(SideTone, CWPitch);
      }
      else
      {
        digitalWrite (CWKeyOutput, HIGH);
        noTone(SideTone);
      }
    }
  }
  // store the state for next time
  LastCWKeyState = CWKeyReading;

  if (LastTXStatus == IN_TX && TXStatus == IN_RX)
  {
    // if we have changed TX state make sure the key is up and
    // also switch off the DDS and disengage the TX relay
    digitalWrite (CWKeyOutput, HIGH);
    DDSDown();
    digitalWrite (TXRXRelay, LOW);
    DisplayTXRX(TXStatus);
  } 
  else if (LastTXStatus == IN_RX && TXStatus == IN_TX)
  {
    digitalWrite (CWKeyOutput, HIGH);
    // if we have transitioned into TX then turn on the DDS
    SetFrequency(TargetFrequency - CWPitch, TargetPhase);
    // and close the TX relay
    digitalWrite (TXRXRelay, HIGH);
    DisplayTXRX(TXStatus);
  }
  
     
  LastTXStatus = TXStatus;
}

void DisplayFreq(unsigned long Frequency) 
{
  float DisplayFrequency;

  DisplayFrequency = (Frequency / 1000000.0); // need .0 here to force cast to float
  
  tft.setTextColor(ILI9341_WHITE,ILI9341_RED);
  tft.setTextSize(5);
  tft.setCursor(30,122);
  tft.print(DisplayFrequency,5);
  tft.print(" M");
  tft.setCursor(30,161);
  tft.setTextColor(ILI9341_BLACK, ILI9341_RED);
  tft.setCursor(90,161);
  switch (DDSStep)
  {
    case 10:
      tft.print("    ^");
      break;
    case 100:
      tft.print("   ^ ");
      break;
    case 1000:  
      tft.print("  ^  "); 
      break;
    case 10000: 
      tft.print(" ^   "); 
      break;
  }
}

void StepSelect()
{
  BtnPress = digitalRead(BTN);

  if (BtnPress != LastBtnPress)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (BtnPress == LOW)
    {
      if (cnt_step == 0) {
        DDSStep = 10; 
      }
      else if (cnt_step == 1) {
        DDSStep = 100; 
      }
      else if (cnt_step == 2) {
        DDSStep = 1000;  
      }
      else if (cnt_step == 3) {
        DDSStep = 10000;  
      }
      cnt_step = cnt_step + 1;
    
      if (cnt_step == 4) {
        cnt_step = 0 ;
      }
    }
  }  
  LastBtnPress = BtnPress;
}

// this only returns when the bit pattern is as expected
// it debounces the rotary encoder well
// A vald CW or  CCW move returns 1 or -1, invalid returns 0
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode <<= 2;
  if (digitalRead(pin_A)) prevNextCode |= 0x02;
  if (digitalRead(pin_B)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;

      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

void rotary_enc()
{

  static int8_t val;

  if( val=read_rotary() ) 
  {
    if ( prevNextCode==0x0b) 
    {
      TargetFrequency = TargetFrequency + DDSStep;
      if (TargetFrequency < VFOLowerLimit) 
      {
        TargetFrequency = VFOUpperLimit;
      }
    }
    if ( prevNextCode==0x07) 
    {
      TargetFrequency = TargetFrequency - DDSStep;
      if (TargetFrequency > VFOUpperLimit) 
      {
        TargetFrequency = VFOLowerLimit;
      }
    }
  }
}

// DDS Routines - these are now generic (I hope)

void DDSInit()
{
  pinMode (DATA, OUTPUT);   // DDS pins as output
  pinMode (CLOCK, OUTPUT);  
  pinMode (FQ_UD, OUTPUT);   
  pinMode (DDSRST, OUTPUT);
  digitalWrite(DATA, LOW);  // internal pull-down
  digitalWrite(CLOCK, LOW);
  digitalWrite(FQ_UD, LOW);
  digitalWrite(DDSRST, LOW);
  
  pulseHigh(DDSRST);
  pulseHigh(CLOCK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode on the AD9850 - Datasheet page 12.  

  // alow the DDS some time after initialisation
  delay (2000);
}

// this is a generic pulse routine 
void pulseHigh(int pin)
{
  digitalWrite(pin, HIGH);
  digitalWrite(pin, LOW);
}

// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void TransferByte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(CLOCK);   //after each bit sent, CLK is pulsed high
  }
}

// pass this frequency in Hz and Phase in degrees
void SetFrequency (uint32_t TargetFrequency, uint8_t TargetPhase)
{
  int32_t freq = TargetFrequency * 4294967295/DDSClock;
  uint8_t phase = TargetPhase << 3;
  UpdateDDS (freq, phase);  
}

// update DDS sends the calculated value to the registers
// a byte at a time the follows with the phase
void UpdateDDS(int32_t freq, uint8_t Phase)
{
  for (int b=0; b<4; b++, freq>>=8) 
  {
    TransferByte(freq & 0xFF);
  }
  TransferByte (Phase & 0xFF);
  pulseHigh(FQ_UD);
}

// this puts the DDS into power down mode
void DDSDown()
{
  pulseHigh(FQ_UD);
  TransferByte(0x04);
  pulseHigh(FQ_UD);
}
