#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define TFT_CS  4
#define TFT_DC  5
#define TFT_RST 12

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const char* ssid     = "";        
const char* password = "";  

WiFiClientSecure client;
String catUrl;

// TJpg_Decoder callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tft.startWrite();
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      tft.drawPixel(x + col, y + row, bitmap[row * w + col]);
    }
  }
  tft.endWrite();
  return true;
}

void setup() {
  Serial.begin(9600);
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setInsecure();

  // Step 1: Get JSON from cataas
  HTTPClient http;
  http.begin(client, "https://cataas.com/cat?json=true");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getStream());
    String url = doc["url"].as<String>();
    // If relative path, prepend domain
    if (url.startsWith("/")) {
      catUrl = "https://cataas.com" + url + "&width=128&height=160";
    } else {
      catUrl = url + "&width=128&height=160";
    }

    Serial.println("Cat image URL: " + catUrl);

  }
  http.end();

  // Step 2: Download JPEG into memory buffer
  HTTPClient imgHttp;
  imgHttp.begin(client, catUrl);
  int imgCode = imgHttp.GET();
  if (imgCode == HTTP_CODE_OK) {
    int len = imgHttp.getSize();
    if (len <= 0) {
      Serial.println("Invalid image size");
      return;
    }

    uint8_t* jpgBuffer = (uint8_t*)malloc(len);
    if (!jpgBuffer) {
      Serial.println("Memory allocation failed");
      return;
    }

    WiFiClient* stream = imgHttp.getStreamPtr();
    int index = 0;
    while (imgHttp.connected() && (index < len)) {
      size_t available = stream->available();
      if (available) {
        int bytesRead = stream->readBytes(jpgBuffer + index, available);
        index += bytesRead;
      }
      delay(1); // yield
    }

    Serial.printf("JPEG downloaded (%d bytes)\n", index);

    // Step 3: Decode and draw
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);
    TJpgDec.drawJpg(0, 0, jpgBuffer, index);

    free(jpgBuffer);
    Serial.println("Cat displayed!");
  } else {
    Serial.printf("Image request failed: %d\n", imgCode);
  }
  imgHttp.end();
}

void loop() {}
