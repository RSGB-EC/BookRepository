const int CLOCK = D5;
const int FQ_UD = D7;
const int DATA = D6;
const int RST = D4;

const uint32_t DDSClock = 125000000;

const uint32_t targetFrequency = 10000000;
const uint8_t targetPhase = 0;

void DDSReset()
{
  digitalWrite(DATA, LOW);
  digitalWrite(FQ_UD, LOW);
  digitalWrite(CLOCK, LOW);

  pulseHigh(RST);
  pulseHigh(CLOCK);
  pulseHigh(FQ_UD);
}

void pulseHigh (int pin)
{
  digitalWrite(pin, HIGH);
  delayMicroseconds(2);
  digitalWrite(pin, LOW);
}

void transferByte (byte data)
{
  for (int i=0; i<=8; i++, data>>=1)
  {
    digitalWrite(DATA, data & 0x01);
    pulseHigh (CLOCK);
  }
}

void updateDDS (int32_t freq, uint8_t phase)
{
  for (int b=0; b<4; b++, freq>>=8)
  {
    transferByte(freq & 0xFF);
  }
  transferByte (phase & 0xFF);
  pulseHigh (FQ_UD);
}

void setFrequency (uint32_t targetFrequency, uint8_t targetPhase)
{
  int32_t freq = targetFrequency * pow(2,32) / DDSClock;
  int8_t phase = targetPhase << 3 & 0x02; // phase in top 5 bits and set clock multiplier

  updateDDS (freq, phase);
}

void setup() {
  // put your setup code here, to run once:
  
  pinMode(DATA, OUTPUT);
  digitalWrite (DATA, LOW);

  pinMode(CLOCK, OUTPUT);
  digitalWrite(CLOCK, LOW);

  pinMode(FQ_UD, OUTPUT);
  digitalWrite(FQ_UD, LOW);

  pinMode(RST, OUTPUT);
  digitalWrite(RST, LOW);

  DDSReset();
}

void loop() {
  // put your main code here, to run repeatedly:
   setFrequency (targetFrequency, targetPhase);
   delay(2000);
}
