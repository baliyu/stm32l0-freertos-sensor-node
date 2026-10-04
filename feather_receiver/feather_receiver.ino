#include <SPI.h>
#include <LoRa.h>

const int PIN_LORA_CS  = 8;
const int PIN_LORA_RST = 4;
const int PIN_LORA_IRQ = 3;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_IRQ);
  if (!LoRa.begin(868.1E6)) {
    Serial.println("LoRa init failed");
    while (1) {}
  }
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  Serial.println("Listening on 868.1 MHz...");
}

void loop() {
  int size = LoRa.parsePacket();
  if (size) {
    String msg;
    while (LoRa.available()) msg += (char)LoRa.read();
    if (msg.startsWith("STM32 ")) {
      Serial.print("RX: ");
      Serial.print(msg);
      Serial.print("  RSSI: ");
      Serial.print(LoRa.packetRssi());
      Serial.print(" dBm  SNR: ");
      Serial.println(LoRa.packetSnr());
    }
  }
}