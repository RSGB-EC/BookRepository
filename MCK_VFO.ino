#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// constants for the upper and lower limits of the radio 
const int_fast32_t VFOUpperLimit = 3800000;
const int_fast32_t VFOLowerLimit = 3500000;

// TFT pins SPI1 plus:
//      Vcc 3.3V                --->  ILI9340 Pin# 1
//      GND                     --->          Pin# 2
const int TFT_CS   =  D10;   // --->          Pin# 3
//      TFT_RST       3V3    // --->          Pin# 4
const int TFT_DC   =  D9;    // --->          Pin# 5
//      TFT_MOSI //   D11       --->          Pin# 6
//      TFT_SCK  //   D13       --->          Pin# 7
//      LED backlight 3V3       --->          Pin# 8
//      TFT_MISO      N/C       --->          Pin# 9

// DDS Pins
const int RES = D5;
const int DATA  = D6;  
const int CLOCK = D2; 
const int LOAD = D3;

void pulseHigh(int pin)
{
  digitalWrite(pin, HIGH);
  digitalWrite(pin, LOW);
}

// rotary encoder pins
const int pin_A = A0;  // this is A0 pin 14
const int pin_B = A1;  // this is A1 pin 15
const int BTN   = A2;  // this is A2 pin 16

// define the TFT instance
// use the hardware SPI interface (much faster)
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

int cnt_step = 0;
int cnt_step_old = 0;
int_fast32_t TargetFrequency = 3550000;
int_fast32_t TargetFrequency_old;

// DDS Clock should be 125,000,000 but mine is a little low
int_fast32_t DDSClock = 125000000; // mine was 124999656 as an adjusted value

int BtnPress = LOW;
int LastBtnPress = LOW;
unsigned long lastDebounceTime = 0;   // the last time the encoder button was toggled
unsigned long debounceDelay = 140;    // the debounce time; increase if the output jumps

unsigned long DDSStep = 10;
unsigned long currentTime;
unsigned long loopTime;
unsigned char encoder_A;
unsigned char encoder_B;
unsigned char encoder_A_prev;

void setup() 
{
  pinMode (DATA, OUTPUT);   // DDS pins as output
  pinMode (CLOCK, OUTPUT);  
  pinMode (LOAD, OUTPUT);   
  pinMode (RES, OUTPUT);
  digitalWrite(DATA, LOW);  // internal pull-down
  digitalWrite(CLOCK, LOW);
  digitalWrite(LOAD, LOW);
  digitalWrite(RES, LOW);
  
  // encoder pins
  pinMode (pin_A,INPUT_PULLUP);
  pinMode (pin_B,INPUT_PULLUP);   
  pinMode (BTN,INPUT_PULLUP);   

  // setup and initialise the screen
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 110, 360, 90, ILI9341_RED);
  tft.setCursor(75, 10);
  tft.setTextColor(ILI9341_RED);    
  tft.setTextSize(5);
  tft.println("SUDDEN");
  tft.setTextColor(ILI9341_GREEN);  
  tft.setCursor(45, 60);
  tft.setTextSize(4);
  tft.println("80M RX VFO");
  
  // give the AD9850 2 seconds after power on
  delay (2000);
  pulseHigh(RES);
  pulseHigh(CLOCK);
  pulseHigh(LOAD);  // this pulse enables serial mode on the AD9850 - Datasheet page 12.

  sendFrequency(TargetFrequency);
}

static uint8_t prevNextCode = 0;
static uint16_t store=0;

void loop() 
{
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
    sendFrequency(TargetFrequency);
    DisplayFreq(TargetFrequency);
  }
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
      if (TargetFrequency > VFOUpperLimit) 
      {
        TargetFrequency = VFOUpperLimit;
      }
      
    }
    if ( prevNextCode==0x07) 
    {
      TargetFrequency = TargetFrequency - DDSStep;
      if (TargetFrequency < VFOLowerLimit) 
      {
        TargetFrequency = VFOLowerLimit;
      }    }
  }
}

// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {  
  // Set frequency
  int32_t freq = frequency * 4294967296/DDSClock;  // note 125 MHz clock on 9850.  You can make 'slight' tuning variations here by adjusting the clock frequency.
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip as we dont care about phase
  pulseHigh(LOAD);   // Done!  Should see output
}
//transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(CLOCK);   //after each bit sent, CLK is pulsed high
  }
}