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
    Serial.println("Waiting for IP...");
    delay(2000);
    return;
  }

  Serial.println("\n[LONG-POLL] Server par Wait kar rahe hain...");
  
  NetworkClientSecure secureClient; 
  HTTPClient http;                  
  secureClient.setInsecure(); 
  
  http.begin(secureClient, serverUrl);
  http.setTimeout(60000); 
  
  const char* headerKeys[] = {"Content-Disposition"};
  http.collectHeaders(headerKeys, 1);
  
  int httpCode = http.GET();
  String ackMessage = ""; // Dashboard ko bhejane ke liye message

  if (httpCode == 200) {
    String finalFilename = "/test_download.bin"; 
    if (http.hasHeader("Content-Disposition")) {
      String disp = http.header("Content-Disposition");
      int start = disp.indexOf("filename=\"");
      if (start != -1) {
        int end = disp.indexOf("\"", start + 10);
        if (end != -1) finalFilename = "/" + disp.substring(start + 10, end);
      }
    }
    
    // SAFE DOWNLOAD: Pehle temporary file me download karo
    String tempFilename = finalFilename + ".tmp";
    Serial.println("✅ Downloading to Temp file: " + tempFilename);
    
    File file = SD.open(tempFilename, FILE_WRITE);
    if (!file) {
      Serial.println("[ERROR] SD Card me file open nahi hui!");
      ackMessage = "Failed_SD_Card_Error";
    } else {
      auto stream = http.getStreamPtr();
      uint8_t buff[512] = { 0 };
      int totalSize = http.getSize();
      int len = totalSize;
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
      
      // CHECK: Download 100% complete hua ya kata?
      if (totalSize > 0 && bytesWritten == totalSize) {
         if (SD.exists(finalFilename)) SD.remove(finalFilename); // Purani file hatao
         SD.rename(tempFilename, finalFilename); // .tmp hatakar Asli naam rakho
         
         Serial.printf("✅ Download 100%% Complete! Saved as %s\n", finalFilename.c_str());
         ackMessage = "Success_Saved_100%";
      } else {
         Serial.println("❌ ERROR: Connection beech me kat gaya! Kharab file delete kar rahe hain.");
         SD.remove(tempFilename); // Corrupt file mitao
         ackMessage = "Failed_Disconnected_in_middle";
      }
    }
  } else if (httpCode == 204) {
    Serial.println("[TIMEOUT] Server par koi file nahi thi.");
  } else {
    Serial.printf("[ERROR] HTTP fail: %d\n", httpCode);
  }
  
  http.end(); // Purana connection band karein

  // DASHBOARD KO NOTIFICATION BHEJEIN
  if (ackMessage != "") {
     HTTPClient ackHttp;
     ackHttp.begin(secureClient, String("https://esp32-ota.onrender.com/ack?msg=") + ackMessage);
     ackHttp.GET();
     ackHttp.end();
  }

  delay(1000); 
}

