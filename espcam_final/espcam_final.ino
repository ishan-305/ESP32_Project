#include "esp_camera.h"
#include <WiFi.h>
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <HTTPClient.h>
#include "base64.h"

// WiFi credentials
const char* ssid = "ISHAN.SINGH.2G";
const char* password = "ishan@123";

// Server endpoints
const char* uploadUrl = "http://192.168.1.12:3000/upload";
const char* lockStateUrl = "http://192.168.1.12:3000/lock-state";

// GPIO definitions
#define LOCK_PIN 12           // Controls electronic lock (TIP122 base)
#define DOORBELL_PIN 14       // Push button to trigger camera

// Camera pin definitions for AI-Thinker model

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_RGB565; // Your cam doesn't support JPEG
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 1;


  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
  }
}

void sendPhoto() {
  // Flush the buffer once
  camera_fb_t* old_fb = esp_camera_fb_get();
  if (old_fb) {
    esp_camera_fb_return(old_fb);  // Return the stale frame
  }

  delay(100); // slight delay helps with getting a fresh frame

  // Now get the actual fresh frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    return;
  }

  Serial.println("Captured fresh image!");

  String imageBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  String body = "{\"image\":\"" + imageBase64 + "\"}";

  HTTPClient http;
  http.begin(uploadUrl);
  http.addHeader("Content-Type", "application/json");
  int responseCode = http.POST(body);

  if (responseCode > 0) {
    Serial.println("Image sent to server.");
  } else {
    Serial.printf("POST failed. Code: %d\n", responseCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, LOW); // default state: locked
  pinMode(DOORBELL_PIN, INPUT_PULLUP); // active LOW
  setupCamera();
}

bool buttonPressed = false;
unsigned long lastButtonCheck = 0;
const int debounceDelay = 50;  // 50 ms debounce

void loop() {
  unsigned long currentMillis = millis();

  // Check button state with debounce
  if (currentMillis - lastButtonCheck > debounceDelay) {
    lastButtonCheck = currentMillis;

    int val = digitalRead(DOORBELL_PIN);
    if (val == LOW && !buttonPressed) {  // active LOW and not already processed
      buttonPressed = true;
      Serial.println("Button pressed. Capturing image...");
      sendPhoto();
    }

    if (val == HIGH) {
      buttonPressed = false;  // reset when button released
    }
  }

  // Poll lock state from server every 2 sec
  static unsigned long lastLockCheck = 0;
  if (currentMillis - lastLockCheck > 2000) {
    lastLockCheck = currentMillis;

    HTTPClient http;
    http.begin(lockStateUrl);
    int code = http.GET();

    if (code == 200) {
      String state = http.getString();
      if (state == "unlock") {
        digitalWrite(LOCK_PIN, HIGH);  // unlock
      } else {
        digitalWrite(LOCK_PIN, LOW);   // lock
      }
    }

    http.end();
  }
}
