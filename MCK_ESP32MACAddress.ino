#include "WiFi.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while(!Serial)
  Serial.println("Starting...");
  WiFi.mode(WIFI_MODE_STA);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  delay(1000);
}
