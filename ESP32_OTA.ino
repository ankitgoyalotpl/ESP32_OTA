#include <Arduino.h>
#include <PPP.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// ==========================================
// PINS CONFIGURATION
// ==========================================
#define PPP_MODEM_TX_PIN 4
#define PPP_MODEM_RX_PIN 5

#define SD_CS 21
#define SD_SCK 12
#define SD_MOSI 11
#define SD_MISO 13

const char* serverUrl = "https://esp32-ota.onrender.com/poll";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nInitializing SD Card...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("[ERROR] SD Card Mount Failed!");
  } else {
    Serial.println("[SUCCESS] SD Card Ready!");
  }

  Serial.println("Starting 4G Internet (PPP)...");
  PPP.setApn("airtelgprs.com");
  PPP.setPins(PPP_MODEM_TX_PIN, PPP_MODEM_RX_PIN, -1, -1, ESP_MODEM_FLOW_CONTROL_NONE);
  
  if (!PPP.begin(PPP_MODEM_SIM7600, 2, 115200)) {  // Aapke hardware ke according
    Serial.println("PPP Failed to start!");
  }

  Serial.print("Connecting to Internet...");
  while (!PPP.attached()) {
    delay(1000);
    Serial.print(".");
  }
  PPP.mode(ESP_MODEM_MODE_DATA);
  delay(3000);
  Serial.println("\n✅ Internet Connected!");
}

void loop() {
  if (PPP.localIP() == IPAddress(0,0,0,0)) {
    Serial.println("Waiting for IP Address...");
    delay(2000);
    return;
  }

  Serial.println("\n[LONG-POLL] Server ko call laga rahe hain (Hold par baithne ke liye)...");
  
  NetworkClientSecure secureClient; 
  HTTPClient http;                  
  
  secureClient.setInsecure(); 
  http.begin(secureClient, serverUrl);
  http.setTimeout(60000); // 60 seconds ka wait
  
  // 👇 1. HTTP Headers me se original naam nikalne ki setting
  const char* headerKeys[] = {"Content-Disposition"};
  http.collectHeaders(headerKeys, 1);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.println("[SUCCESS] Server se file milna shuru ho gayi hai!");
    
    // 👇 2. Asli naam nikalna
    String filename = "/test_download.bin"; // Default naam
    if (http.hasHeader("Content-Disposition")) {
      String disp = http.header("Content-Disposition");
      int start = disp.indexOf("filename=\"");
      if (start != -1) {
        int end = disp.indexOf("\"", start + 10);
        if (end != -1) filename = "/" + disp.substring(start + 10, end);
      }
    }
    
    Serial.println("✅ File ab is naam se save ho rahi hai: " + filename);
    
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("[ERROR] SD Card me file open nahi ho payi!");
    } else {
      auto stream = http.getStreamPtr();
      uint8_t buff[512] = { 0 };
      int len = http.getSize();
      int bytesWritten = 0;
      
      while (http.connected() && (len > 0 || len == -1)) {
        int size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          file.write(buff, c);
          bytesWritten += c;
          if (len > 0) len -= c;
        }
        delay(1); 
      }
      file.close();
      Serial.printf("✅ Jadoo! File Render se direct SD card me save ho gayi! Total Size: %d bytes\n", bytesWritten);
    }
  } else if (httpCode == 204) {
    Serial.println("[TIMEOUT] Server par abhi tak koi file nahi thi. Wapas call lagayenge...");
  } else {
    Serial.printf("[ERROR] HTTP fail ho gaya. Error code: %d\n", httpCode);
  }
  
  http.end();
  delay(1000);
}
