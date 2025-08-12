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
#define BUTTON_PIN 0  // GPIO0 = D3

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const char* ssid     = "BTHub6-T6HX";        
const char* password = "9vqUKrRMURbn";     


WiFiClientSecure client;
String catUrl;

unsigned long lastCatTime = 0;
const unsigned long catInterval = 5000; // 5 seconds

// Faster TJpg_Decoder callback using block writes
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height() || x >= tft.width()) return false;

  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  tft.writePixels(bitmap, w * h, true); // true = big-endian swap
  tft.endWrite();
  return true;
}

void showCat() {
  // Step 1: Get JSON from cataas
  HTTPClient http;
  http.begin(client, "https://cataas.com/cat?json=true");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getStream());
    String url = doc["url"].as<String>();

    // Fix: only prepend domain if relative
    if (url.startsWith("/")) {
      catUrl = "https://cataas.com" + url;
    } else {
      catUrl = url;
    }

    // Add width & height params
    if (catUrl.indexOf('?') >= 0) {
      catUrl += "&width=128&height=160";
    } else {
      catUrl += "?width=128&height=160";
    }

    Serial.println("Cat image URL: " + catUrl);
  }
  http.end();

  // Step 2: Download JPEG into buffer
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
      delay(1);
    }

    Serial.printf("JPEG downloaded (%d bytes)\n", index);

    // Step 3: Decode & display
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false); 
    TJpgDec.setCallback(tft_output);
    TJpgDec.drawJpg(0, 0, jpgBuffer, index);

    free(jpgBuffer);
    Serial.println("Cat displayed!");
  } else {
    Serial.printf("Image request failed: %d\n", imgCode);
  }
  imgHttp.end();
}

void setup() {
  Serial.begin(9600);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0); // Portrait
  tft.fillScreen(ST77XX_BLACK);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setInsecure(); // Ignore cert check
  showCat(); // First cat
  lastCatTime = millis();
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed!");
      showCat(); // fetch new cat immediately
    }

}
