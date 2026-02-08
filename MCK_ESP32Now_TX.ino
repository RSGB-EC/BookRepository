/*
ESP-Now TX
*/

// Include Libraries
#include <esp_now.h>
#include <WiFi.h>
#include <cstring>   // for strcpy, memcpy (optional on Arduino, but nice to be explicit)

int int_value;
float float_value;
bool bool_value = true;

// MAC Address of responder
uint8_t broadcastAddress[] = {0xA0, 0xB7, 0x65, 0x21, 0x75, 0x00};

// Define a data structure
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;

// Create a structured object
struct_message myData;

// Peer info
esp_now_peer_info_t peerInfo = {};

// callback function
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

  // If you want the destination MAC (new core):
  if (info) {
    Serial.print("To: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", info->des_addr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
  }
}

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Optional: seed randomness
  randomSeed(esp_random());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the send callback (PASS FUNCTION POINTER, DON'T CALL IT)
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;      // 0 = current WiFi channel
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  // Generate a random integer
  int_value = random(1, 20);

  // Use integer to make a new float
  float_value = 1.3f * int_value;

  // Invert the boolean value
  bool_value = !bool_value;

  // Format structured data
  strcpy(myData.a, "Experimenters' Corner!");
  myData.b = int_value;
  myData.c = float_value;
  myData.d = bool_value;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("Sending OK");
  } else {
    Serial.println("Sending Fail");
  }

  delay(2000);
}
