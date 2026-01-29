/*
  NUCLEO-L432KC - A0 (PA0) 12-bit ADC sampling + Trimmed Mean + dBm conversion

  dBm = ADCReading * 0.0326 – 94.937

  - Collect 100 samples from A0, 10 ms apart
  - Sort samples
  - Remove both low and high outliers (trimmed mean)
  - Convert trimmed mean ADC value to dBm
  - Print results to Serial
  - Wait 1 second and repeat
*/

#include <Arduino.h>

static const uint16_t N_SAMPLES = 100;
static const uint16_t SAMPLE_DELAY_MS = 10;
static const uint16_t REPORT_DELAY_MS = 1000;

// Remove 10 lowest + 10 highest
static const uint16_t TRIM_COUNT = 10;

static_assert((2 * TRIM_COUNT) < N_SAMPLES, "TRIM_COUNT too large");

uint16_t samples[N_SAMPLES];

// --------------------------------------------------
// Insertion Sort
// --------------------------------------------------
static void insertionSort(uint16_t *arr, uint16_t n) {
  for (uint16_t i = 1; i < n; i++) {
    uint16_t key = arr[i];
    int16_t j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

// --------------------------------------------------
// Trimmed Mean
// --------------------------------------------------
static uint16_t trimmedMean(uint16_t *arr, uint16_t n, uint16_t trim) {
  insertionSort(arr, n);

  uint32_t sum = 0;
  uint16_t count = n - (2 * trim);

  for (uint16_t i = trim; i < (n - trim); i++) {
    sum += arr[i];
  }

  return (uint16_t)((sum + count / 2) / count); // rounded
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  analogReadResolution(12);   // 0..4095
  pinMode(A0, INPUT_ANALOG);

  Serial.println("NUCLEO-L432KC - Trimmed Mean + dBm Conversion");
}

void loop() {

  // Acquire samples
  for (uint16_t i = 0; i < N_SAMPLES; i++) {
    samples[i] = analogRead(A0);
    delay(SAMPLE_DELAY_MS);
  }

  // Remove outliers
  uint16_t adcValue = trimmedMean(samples, N_SAMPLES, TRIM_COUNT);

  // Convert ADC -> dBm
  float dBm = (adcValue * 0.0326f) - 94.937f;

  // Report
  Serial.print("Trimmed Mean ADC = ");
  Serial.print(adcValue);

  Serial.print(" | Signal Level = ");
  Serial.print(dBm, 2);
  Serial.println(" dBm");

  delay(REPORT_DELAY_MS);
}
