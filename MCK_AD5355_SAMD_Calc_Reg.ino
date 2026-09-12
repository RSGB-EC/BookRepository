
#include <SPI.h>
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

#define ADF5355_LE 6

SPISettings spisettings(1000000, MSBFIRST, SPI_MODE0);

int noReg = 13;

struct ADF5355Config {
  uint64_t rfOutHz;          // Desired RF output frequency
  uint64_t refInHz;          // Reference input frequency
  uint32_t channelSpacingHz; // Desired channel spacing

  // Reference path
  uint16_t rCounter;         // 1..1023
  bool refDoubler;           // D bit
  bool refDiv2;              // T bit

  // Output / misc
  bool feedbackFundamental;  // true = feedback from VCO, false = divided feedback
  uint8_t outputPower;       // 0..3
  bool rfEnableA;            // Enable RF output A path
  bool rfEnableB;            // Enable RF output B path
  bool muteTillLockDetect;   // MTLD

  // Register 4 defaults
  bool counterReset;
  bool cpThreeState;
  bool powerDown;
  bool pdPolarityPositive;
  bool muxLogic;
  bool refModeDiff;
  uint8_t chargePumpCurrent; // 0..15
  bool rfDividerDoubleBuffer;

  // Register 7/9/10/12 policy
  uint32_t r7Template;       // Usually 0x120000E7
  uint32_t r9Template;       // Common template default
  bool enableADC;            // R10 DB4
  bool enableADCConversion;  // R10 DB5
  bool autocal;              // R0 DB21
};

struct ADF5355Result {
  uint32_t reg[13];
  uint8_t rfDividerSel;   // 0..6 => /1 /2 /4 /8 /16 /32 /64
  uint32_t rfDivider;     // 1..64
  uint16_t INTval;
  uint32_t FRAC1val;      // 24-bit
  uint16_t FRAC2val;      // 14-bit
  uint16_t MOD2val;       // 14-bit
  double fPFD;
  double vcoHz;
};

static const uint32_t MOD1 = 16777216UL; // 2^24

uint32_t registers[13] =  { 0x00200580, 0x00000001, 0x00000022, 0x00000003, 0x03200A584, 0x00800025, 0x15220476, 
                            0x120000E7, 0x120D0428, 0x0302FCC9, 0x00C01F7A, 0x00061300B, 0x0001041C} ; // init values are 2200MHz 100Mhz ref clock

static uint32_t gcd_u32(uint32_t a, uint32_t b) 
{
  while (b != 0) 
  {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static uint8_t dividerSelFromValue(uint32_t div) 
{
  switch (div) 
  {
    case 1:  return 0;
    case 2:  return 1;
    case 4:  return 2;
    case 8:  return 3;
    case 16: return 4;
    case 32: return 5;
    case 64: return 6;
    default: return 0xFF;
  }
}

static bool chooseOutputDivider(uint64_t rfOutHz, uint32_t &divider, uint8_t &sel, double &vcoHz) 
{
  const uint32_t candidates[] = {1, 2, 4, 8, 16, 32, 64};

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) 
  {
    uint32_t d = candidates[i];
    double vco = (double)rfOutHz * (double)d;
    if (vco >= 3400000000.0 && vco <= 6800000000.0) 
    {
      divider = d;
      sel = dividerSelFromValue(d);
      vcoHz = vco;
      return true;
    }
  }
  return false;
}

static uint8_t selectPrescaler(double vcoHz) 
{
  // 4/5 allowed up to 7 GHz; otherwise use 8/9
  return (vcoHz > 7000000000.0) ? 1 : 0;
}

static uint8_t computeAdcClkDiv(double fPFD_Hz) 
{
  // Matches common practice:
  // ADC clock = fPFD / (4 * (ADC_CLK_DIV + 2))
  // Datasheet examples typically end up around ~100 kHz ADC clock.
  double div = ceil((fPFD_Hz / 100000.0 - 2.0) / 4.0);
  if (div < 1.0) div = 1.0;
  if (div > 255.0) div = 255.0;
  return (uint8_t)div;
}

bool calcADF5355(const ADF5355Config &cfg, ADF5355Result &out) 
{
  memset(&out, 0, sizeof(out));

  // Basic range check
  if (cfg.rfOutHz < 54000000ULL || cfg.rfOutHz > 13600000000ULL) 
  {
    return false;
  }
  
  if (cfg.rCounter < 1 || cfg.rCounter > 1023) 
  {
    return false;
  }
  
  if (cfg.channelSpacingHz == 0) 
  {
    return false;
  }

  // Choose RF output divider so VCO stays within 3.4..6.8 GHz
  if (!chooseOutputDivider(cfg.rfOutHz, out.rfDivider, out.rfDividerSel, out.vcoHz)) 
  {
    return false;
  }

  // Compute PFD
  out.fPFD = (double)cfg.refInHz *
             ((cfg.refDoubler ? 2.0 : 1.0) /
              ((double)cfg.rCounter * (cfg.refDiv2 ? 2.0 : 1.0)));

  if (out.fPFD <= 0.0) {
    return false;
  }

  // N = VCO / fPFD
  const double N = out.vcoHz / out.fPFD;
  const uint8_t prescaler = selectPrescaler(out.vcoHz);
  const uint16_t intMin = prescaler ? 75 : 23;

  out.INTval = (uint16_t)floor(N);
  if (out.INTval < intMin) {
    return false;
  }

  const double frac = N - (double)out.INTval;

  // FRAC1 = floor(MOD1 * frac)
  double frac1Real = frac * (double)MOD1;
  if (frac1Real < 0.0) frac1Real = 0.0;

  out.FRAC1val = (uint32_t)floor(frac1Real);
  if (out.FRAC1val >= MOD1) 
  {
    out.FRAC1val = MOD1 - 1;
  }

  const double remainder = frac1Real - (double)out.FRAC1val;

  // MOD2 = fPFD / GCD(fPFD, channelSpacing)
  // Use integer-Hz math for GCD.
  uint32_t fPFD_int = (uint32_t)llround(out.fPFD);
  uint32_t gcdVal = gcd_u32(fPFD_int, cfg.channelSpacingHz);
  if (gcdVal == 0) 
  {
    return false;
  }

  out.MOD2val = (uint16_t)(fPFD_int / gcdVal);

  // Datasheet says MOD2 must be < 16383 for exact channel-spacing solution.
  if (out.MOD2val < 2 || out.MOD2val > 16383) 
  {
    return false;
  }

  // FRAC2 = remainder * MOD2
  out.FRAC2val = (uint16_t)llround(remainder * (double)out.MOD2val);

  if (out.FRAC2val >= out.MOD2val) 
  {
    out.FRAC2val = out.MOD2val - 1;
  }

  // If exactly integer-N, force FRAC fields to zero and keep legal MOD2.
  if (frac == 0.0) 
  {
    out.FRAC1val = 0;
    out.FRAC2val = 0;
    out.MOD2val  = 2;
  }

  // ---------- Pack registers ----------

  // R0: INT + prescaler + autocal
  out.reg[0] =
      ((uint32_t)out.INTval << 4) |
      ((uint32_t)prescaler << 20) |
      ((uint32_t)(cfg.autocal ? 1 : 0) << 21) |
      0x0;

  // R1: FRAC1
  out.reg[1] =
      ((uint32_t)(out.FRAC1val & 0xFFFFFFUL) << 4) |
      0x1;

  // R2: FRAC2[31:18], MOD2[17:4]
  out.reg[2] =
      ((uint32_t)(out.FRAC2val & 0x3FFFU) << 18) |
      ((uint32_t)(out.MOD2val  & 0x3FFFU) << 4)  |
      0x2;

  // R3: PHASE=0, phase adjust/resync off, SD load reset default off
  out.reg[3] = 0x00000003UL;

  // R4
  out.reg[4] =
      ((uint32_t)(cfg.counterReset ? 1 : 0) << 4)  |
      ((uint32_t)(cfg.cpThreeState ? 1 : 0) << 5)  |
      ((uint32_t)(cfg.powerDown ? 1 : 0) << 6)     |
      ((uint32_t)(cfg.pdPolarityPositive ? 1 : 0) << 7) |
      ((uint32_t)(cfg.muxLogic ? 1 : 0) << 8)      |
      ((uint32_t)(cfg.refModeDiff ? 1 : 0) << 9)   |
      ((uint32_t)(cfg.chargePumpCurrent & 0x0F) << 10) |
      ((uint32_t)(cfg.rfDividerDoubleBuffer ? 1 : 0) << 14) |
      ((uint32_t)(cfg.rCounter & 0x03FF) << 15)    |
      ((uint32_t)(cfg.refDiv2 ? 1 : 0) << 25)      |
      ((uint32_t)(cfg.refDoubler ? 1 : 0) << 26)   |
      ((uint32_t)6 << 27) |   // MUXOUT = digital lock detect
      0x4;

  // R5: fixed per datasheet
  out.reg[5] = 0x00800025UL;

  // R6
  // Bleed current field here uses a common mid/high default (126).
  const uint8_t bleedCurrent = 126;
  out.reg[6] =
      ((uint32_t)(cfg.outputPower & 0x3) << 4) |
      ((uint32_t)(cfg.rfEnableA ? 1 : 0) << 6) |
      ((uint32_t)(cfg.rfEnableB ? 1 : 0) << 10) |
      ((uint32_t)(cfg.muteTillLockDetect ? 1 : 0) << 11) |
      ((uint32_t)bleedCurrent << 13) |
      ((uint32_t)(out.rfDividerSel & 0x7) << 21) |
      ((uint32_t)(cfg.feedbackFundamental ? 1 : 0) << 24) |
      ((uint32_t)10 << 25) |   // gated/negative bleed template default
      0x6;

  // R7: template default; user can change if needed
  out.reg[7] = cfg.r7Template;

  // R8: fixed per datasheet
  out.reg[8] = 0x102D0428UL;

  // R9: template default; user can tune lock-time behavior here
  out.reg[9] = cfg.r9Template;

  // R10: ADC clock divider and ADC enables
  const uint8_t adcClkDiv = computeAdcClkDiv(out.fPFD);
  out.reg[10] =
      (3UL << 22) |                // reserved bits required by datasheet
      ((uint32_t)adcClkDiv << 6) |
      ((uint32_t)(cfg.enableADCConversion ? 1 : 0) << 5) |
      ((uint32_t)(cfg.enableADC ? 1 : 0) << 4) |
      0xA;

  // R11: fixed per datasheet
  out.reg[11] = 0x0061300BUL;

  // R12: normal operation => phase resync clock divider = 1
  out.reg[12] = 0x0001041CUL;

  return true;
}

void WriteRegister32(const uint32_t value)   
{
  digitalWrite(ADF5355_LE, LOW);
  for (int i = 3; i >= 0; i--) 
  {            // loop round 4 x 8bits
    SPI.beginTransaction(spisettings);
    SPI.transfer((value >> 8 * i) & 0xFF); // offset, byte mask and send via SPI
  }
  digitalWrite(ADF5355_LE, HIGH);
  delayMicroseconds(2);
  digitalWrite(ADF5355_LE, LOW);
}

void SetADF5355()  // bung the data into the ADF4351
{ for (int i = noReg-1; i >= 0; i--)
    WriteRegister32(registers[i]);
}

static void printResult(const ADF5355Result &r) {
  SerialUSB.println();
  SerialUSB.println(F("ADF5355 register words:"));
  for (int i = 12; i >= 0; --i) {
    SerialUSB.print(F("R"));
    SerialUSB.print(i);
    SerialUSB.print(F(" = 0x"));
    if (r.reg[i] < 0x10000000UL) SerialUSB.print('0');
    SerialUSB.println(r.reg[i], HEX);
  }

  SerialUSB.println();
  SerialUSB.print(F("fPFD   = "));
  SerialUSB.println(r.fPFD, 3);
  SerialUSB.print(F("VCO Hz = "));
  SerialUSB.println(r.vcoHz, 3);
  SerialUSB.print(F("INT    = "));
  SerialUSB.println(r.INTval);
  SerialUSB.print(F("FRAC1  = "));
  SerialUSB.println(r.FRAC1val);
  SerialUSB.print(F("FRAC2  = "));
  SerialUSB.println(r.FRAC2val);
  SerialUSB.print(F("MOD2   = "));
  SerialUSB.println(r.MOD2val);
  SerialUSB.print(F("RF div = /"));
  SerialUSB.println(r.rfDivider);
}

void setup() 
{
  SerialUSB.begin(115200);
  while (!SerialUSB) { }
  SerialUSB.println("Starting...");

  ADF5355Config cfg;

  cfg.rfOutHz              = 2200000000ULL;   // 2200.00MHz example
  cfg.refInHz              = 100000000ULL;    // 100.00 MHz reference
  cfg.channelSpacingHz     = 200000;          // 200 kHz

  cfg.rCounter             = 1;
  cfg.refDoubler           = false;
  cfg.refDiv2              = true;            // gives 61.44 MHz PFD here

  cfg.feedbackFundamental  = true;
  cfg.outputPower          = 3;
  cfg.rfEnableA            = true;
  cfg.rfEnableB            = true;
  cfg.muteTillLockDetect   = false;

  cfg.counterReset         = false;
  cfg.cpThreeState         = false;
  cfg.powerDown            = false;
  cfg.pdPolarityPositive   = true;
  cfg.muxLogic             = true;
  cfg.refModeDiff          = false;
  cfg.chargePumpCurrent    = 9;
  cfg.rfDividerDoubleBuffer = false;

  cfg.r7Template           = 0x120000E7UL;
  cfg.r9Template           = 0x0302FCC9UL;
  cfg.enableADC            = true;
  cfg.enableADCConversion  = true;
  cfg.autocal              = true;

  ADF5355Result result;
  if (!calcADF5355(cfg, result)) {
    SerialUSB.println(F("ADF5355 calculation failed. Check frequency/reference/settings."));
    return;
  }

  printResult(result);

  for (int i = 12; i >= 0; --i) 
  {
    registers[i] = result.reg[i];
  }

  pinMode(ADF5355_LE, OUTPUT);          
  digitalWrite(ADF5355_LE, LOW);
  SPI.begin();
  delay(5000);
  SetADF5355(); 
}
  

void loop() 
{
  delay (2000);

  SetADF5355();  
}