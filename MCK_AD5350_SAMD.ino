
#include <SPI.h>

#define ADF5350_LE 6

SPISettings spisettings(1000000, MSBFIRST, SPI_MODE0);

int noReg = 13;

uint32_t registers[13] =  {0x2002C0, 0x0000001, 0x1F42, 0x3, 0x3000A784, 0x800025, 0x15220476, 0x120000E7,
0x102D0428, 0x302FCC9, 0xC03EBA, 0x61300B, 0x1041C} ; // 2200MHz 100Mhz ref clock

void WriteRegister32(const uint32_t value)   
{
  digitalWrite(ADF5350_LE, LOW);
  for (int i = 3; i >= 0; i--) 
  {            // loop round 4 x 8bits
    SPI.beginTransaction(spisettings);
    SPI.transfer((value >> 8 * i) & 0xFF); // offset, byte mask and send via SPI
  }
  digitalWrite(ADF5350_LE, HIGH);
  delayMicroseconds(2);
  digitalWrite(ADF5350_LE, LOW);
}

void SetADF5350()  // bung the data into the ADF4351
{ for (int i = noReg-1; i >= 0; i--)
    WriteRegister32(registers[i]);
}

void setup() 
{
  Serial.begin (9600);

  pinMode(ADF5350_LE, OUTPUT);          
  digitalWrite(ADF5350_LE, LOW);
  SPI.begin();
  delay(5000);
}
  

void loop() 
{
  delay (2000);

  SetADF5350();  
}