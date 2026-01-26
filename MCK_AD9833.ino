// include the library
#include <MD_AD9833.h>

// Pins for comms with the AD9833 IC
const int DATA  = D10;  // Data 
const int CLK   = D11;  // Clock
const int FSYNC = D12;  // Load   (FSYNC pin on AD9833)

//MD_AD9833 SigGen(FSYNC);  // Hardware SPI
MD_AD9833 SigGen(DATA, CLK, FSYNC); // use pins defined above for interfacing

void setup(void)
{
  // call the library routine to initialise the AD9833 Signal Generator
  // to a 1KHz Sine Wave
  SigGen.begin();

}

void loop(void)
{
  // set the frequency on the main channel to be 1500Hz
  SigGen.setFrequency(MD_AD9833::CHAN_0,1500);
  // set the output type to be Sine
  SigGen.setMode(MD_AD9833::MODE_SINE);
  // hold the boat for 3 seconds
  delay(3000);
  
  // set the frequency on the main channel to be 2000Hz
  SigGen.setFrequency(MD_AD9833::CHAN_0,2000);
  // now set the output type to be a triangle
  SigGen.setMode(MD_AD9833::MODE_TRIANGLE);
  delay(3000);
  
  // set the frequency on the main channel to be 3000Hz
  SigGen.setFrequency(MD_AD9833::CHAN_0,3000);
  // and now a square wave
  SigGen.setMode(MD_AD9833::MODE_SQUARE1);
  delay(3000);
}
