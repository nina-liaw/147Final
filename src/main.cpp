#include <Arduino.h>
// #include <HTTPClient.h> // was Http
#include <HttpClient.h> // was Http

#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "Wire.h"
#include "SparkFunLSM6DSO.h"
#include <SPI.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <TFT_eSPI.h>
#include <Adafruit_CAP1188.h>
#include <ArduinoJson.h>
#include <vector>

#define ADD_BUTTON_PIN 15
#define SUBTRACT_BUTTON_PIN 13

LSM6DSO IMU;
TFT_eSPI tft = TFT_eSPI();
Adafruit_CAP1188 cap = Adafruit_CAP1188(25, 33, 32, 26, 21);

// Define your server details
const char serverAddress[] = "3.145.206.105"; // Replace with your Flask server's IP or domain
const int serverPort = 5000;
const char postPath[] = "/"; // Replace with your Flask endpoint

WiFiClient c;
HttpClient http(c);

bool calibrating = true;
bool firstCalibration = true;
unsigned long calibCounter;
unsigned long start;
float max_accel = INT_MIN;
int accel_total = 0;
int steps = 0;
double waterCounter = 0;

bool goalsSet = false;
int goalSteps = 0;
int goalWater = 0;
std::vector<int> stepCounts;
std::vector<int> waterCounts;
std::vector<long> timestamps;
long time_stamp = 0;

int timeBlock = 120000; // 2 min

int dataInterval = 5000; // 5s
long startDataInterval = -1;

// bools
bool displayStepReminder = true;
bool displayWaterReminder = true;
bool displayStepMessage = true;
bool displayWaterMessage = true;

// This example downloads the URL "http://arduino.cc/"
char ssid[50]; // your network SSID (name)
char pass[50]; // your network password (use for WPA, or use as key for WEP)

// Name of the server we want to connect to
const char kHostname[] = "worldtimeapi.org";
// Path to download (this is the bit after the hostname in the URL
// that you want to download
const char kPath[] = "/api/timezone/Europe/London.txt";

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;



void nvs_access() {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open
    Serial.printf("\n");
    Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, & my_handle);

    if (err != ESP_OK) {
        Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        Serial.printf("Done\n");
        Serial.printf("Retrieving SSID/PASSWD\n");
        size_t ssid_len;
        size_t pass_len;
        err = nvs_get_str(my_handle, "ssid", ssid, & ssid_len);
        err |= nvs_get_str(my_handle, "pass", pass, & pass_len);
        switch (err) {
        case ESP_OK:
            Serial.printf("Done\n");
            //Serial.printf("SSID = %s\n", ssid);
            //Serial.printf("PASSWD = %s\n", pass);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            Serial.printf("The value is not initialized yet!\n");
            break;
        default:
            Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
    }
    // Close
    nvs_close(my_handle);
}

void setup() {
    Serial.begin(115200);

    pinMode(ADD_BUTTON_PIN, INPUT);
    pinMode(SUBTRACT_BUTTON_PIN, INPUT);

    Wire.begin();
    if(IMU.begin() )
      Serial.println("Ready.");
    else { 
      Serial.println("Could not connect to IMU.");
      Serial.println("Freezing");
    }

    if (IMU.initialize(BASIC_SETTINGS) )
      Serial.println("Loaded Settings.");

    delay(1000);
    // Retrieve SSID/PASSWD from flash before anything else
    nvs_access();
    // We start by connecting to a WiFi network
    delay(1000);
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(WiFi.macAddress());

    // DISPLAY SCREEN
    Serial.println("turning on screen");
    tft.init();
    cap.begin();
    tft.fillScreen(TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    
}



void loop() {
    int err = 0;
    
    // err = http.get(kHostname, kPath);
    char url[100];
    float accel_init = IMU.readFloatAccelX();
    
    if (!goalsSet) {
      bool validStepGoal = false;
      while (!validStepGoal) {
          Serial.println("Enter your step goal >> ");
          
          // Clear any leftover characters in the buffer
          while (Serial.available()) {
              Serial.read();
          }
          
          String input = "";

          while (true) {
              if (Serial.available() > 0) {
                  char c = Serial.read();
                  if (c == '\n' || c == '\r') {
                      break;
                  }
                  input += c;
              }
              delay(10);
          }

          goalSteps = input.toInt();
          if (goalSteps > 0) {
              Serial.print("Your step goal is set to: ");
              Serial.println(goalSteps);
              validStepGoal = true;
          } else {
              Serial.println("Invalid input. Please enter a valid number.");
          }
      }

      delay(200);

      bool validWaterGoal = false;
      while (!validWaterGoal) {
          Serial.println("Enter your water goal >> ");
          
          // Clear any leftover characters in the buffer
          while (Serial.available()) {
              Serial.read();
          }
          
          String input2 = "";

          while (true) {
              if (Serial.available() > 0) {
                  char c = Serial.read();
                  if (c == '\n' || c == '\r') {
                      break;
                  }
                  input2 += c;
              }
              delay(10);
          }

          goalWater = input2.toInt();
          if (goalWater > 0) {
              Serial.print("Your water goal is set to: ");
              Serial.println(goalWater);
              validWaterGoal = true;
              goalsSet = true;
          } else {
              Serial.println("Invalid input. Please enter a valid number.");
          }
      }
      
      start = millis();
  }
    
    
    if (calibrating) {
      if (firstCalibration) {
        Serial.print("Calibrating...");
        firstCalibration = false;
        calibCounter = millis();
      }
      if (millis() - calibCounter > 800) {
        Serial.print(".");
        calibCounter = millis();
      }
      
      
      accel_init = IMU.readFloatAccelX();
      
      if (accel_init > max_accel) {
        max_accel = accel_init;
      }

      // Serial.println(max_accel);
      if (millis() - start > 10000) { // 10s
        calibrating = false;
      }
    }
    else {
      if (!calibCounter) {
        Serial.println("");
        calibCounter = true;
      }
      // Serial.println("max_accel");
      // Serial.println(max_accel);

      if (startDataInterval == -1) { // set for the first time
        startDataInterval = millis();
      }

      float accel = abs(IMU.readFloatAccelX());
      // Serial.println(accel);

      if (accel >= max_accel * 0.5) {
        accel_total++;
        steps = accel_total;
      }

      // Serial.println(accel_total);
            // Serial.println(steps);
      // Serial.print("steps: >> ");
      // Serial.println(steps);

      // delay(20);

      if (digitalRead(ADD_BUTTON_PIN) == HIGH) {
        waterCounter++;
        // Serial.println("adding");
        // Serial.print("ADD waterCounter: >> ");
        // Serial.println(waterCounter);
      }

      if (digitalRead(SUBTRACT_BUTTON_PIN) == HIGH) {
        waterCounter -= 0.5;
        // Serial.print("MINUS waterCounter: >>");
        // Serial.println(waterCounter);
      }

      
      // float accel_init = IMU.readFloatAccelX();
      // Serial.print("accel: ");
      // Serial.println(accel_init);

      // SENDING DATA

      // if 5s have passed since the last data save
      if (millis() - startDataInterval > dataInterval) { 
        // save data
        // dataPoints++;
        stepCounts.push_back(steps);
        waterCounts.push_back(waterCounter);
        timestamps.push_back(time_stamp);
        time_stamp += 5;
        startDataInterval = millis();
      }
      

      // Create JSON object
      // StaticJsonDocument<1024> jsonDoc; ///
      DynamicJsonDocument jsonDoc(2048);
      JsonArray stepArray = jsonDoc.createNestedArray("steps");
      JsonArray waterArray = jsonDoc.createNestedArray("water");

      for (int i = 0; i < stepCounts.size(); i++) {
          JsonObject stepObj = stepArray.createNestedObject();
          stepObj["timestamp"] = timestamps.at(i);
          stepObj["count"] = stepCounts.at(i);

          JsonObject waterObj = waterArray.createNestedObject();
          waterObj["timestamp"] = timestamps.at(i);
          waterObj["count"] = waterCounts.at(i);
      }

      // Serialize JSON to string
      String jsonPayload;
      serializeJson(jsonDoc, jsonPayload);

      // Serial.println(jsonPayload);
      http.beginRequest();

      int responseCode = http.post(serverAddress, serverPort, postPath, NULL);

      http.sendHeader("Content-Type", "application/json");
      http.sendHeader("Content-Length", jsonPayload.length());
      http.print(jsonPayload); // Send JSON as the body of the request
      http.endRequest();

      http.write((const uint8_t*)jsonPayload.c_str(), jsonPayload.length());


      // Serial.print("responseCode: ");
      // Serial.println(responseCode);
      // Read the response
      err = http.responseStatusCode();
      // String response = http.responseBody();
      

      if (err >= 0) {
          // Serial.print("Got status code: ");
          // Serial.println(err);
          // Serial.print("Response: ");
          // Serial.println(response);
      } else {
          Serial.print("Error receiving response: ");
          Serial.println(err);
      }
      

      // if (err == 0) {
      //     Serial.println("startedRequest ok");
      //     err = http.responseStatusCode();
      //     if (err >= 0) {
      //         Serial.print("Got status code: ");
      //         Serial.println(err);
      //         // Usually you'd check that the response code is 200 or a
      //         // similar "success" code (200-299) before carrying on,
      //         // but we'll print out whatever response we get
      //         err = http.skipResponseHeaders();
      //         if (err >= 0) {
      //             // int bodyLen = http.contentLength();
      //             // Serial.print("Content length is: "); ///
      //             // Serial.println(bodyLen);
      //             // Serial.println();
      //             // Serial.println("Body returned follows:"); ///
      //             // // Now we've got to the body, so we can print it out
      //             // unsigned long timeoutStart = millis();
      //             // char c;
      //             // // Whilst we haven't timed out & haven't reached the end of the body
      //             // while ((http.connected() || http.available()) &&
      //             //     ((millis() - timeoutStart) < kNetworkTimeout)) {
      //             //     if (http.available()) {
      //             //         c = http.read();
      //             //         // Print out this character
      //             //         // Serial.print(c); ///
      //             //         bodyLen--;
      //             //         // We read something, reset the timeout waterCounter
      //             //         timeoutStart = millis();
      //             //     } else {
      //             //         // We haven't got any data, so let's pause to allow some to arrive
      //             //         delay(kNetworkDelay);
      //             //     }
      //             // }
      //         } else {
      //             Serial.print("Failed to skip response headers: ");
      //             Serial.println(err);
      //         }
      //     } else {
      //         Serial.print("Getting response failed: ");
      //         Serial.println(err);
      //     }
      // } else {
      //     Serial.print("Connect failed: ");
      //     Serial.println(err);
      // }
      http.stop();


      // checking if goal met
      if (steps >= goalSteps && displayStepMessage) {
        Serial.println("\n\nCongratulations on reaching your step goal!!");
        displayStepMessage = false;
      }
      // check ins
      else if (millis() - start > timeBlock && displayStepMessage) {
        Serial.println("\n\nYou didn't meet your step goal today :(");
        displayStepMessage = false;
      }
      else if (millis() - start > timeBlock/2 && displayStepReminder && steps < goalSteps) {
        Serial.println("\n\nGet moving!!");
        displayStepReminder = false;
      }

      // checking if goal met
      if (waterCounter >= goalWater && displayWaterMessage) {
        Serial.println("\n\nCongratulations on reaching your water goal!!");
        displayWaterMessage = false;
      }
      // check ins
      else if (millis() - start > timeBlock && displayWaterMessage) {
        Serial.println("\n\nYou didn't meet your water goal today :(");
        displayWaterMessage = false;
      }
      else if (millis() - start > timeBlock/2 && displayWaterReminder && waterCounter < goalWater) {
        Serial.println("\n\nRemember to drink some water!!");
        displayWaterReminder = false;
      }

      // Serial.println("drawing");
    //   tft.println("steps: ");
      tft.drawNumber(steps, 10, 20, 4);
    //   tft.drawCentreString("\n", 10, 20, 4);
    //   tft.drawNumber(waterCounter, 10, 40, 4);
      tft.drawFloat(waterCounter, 1, 10, 40, 4);

    //   String count = std::to_string(waterCounter);
    //   tft.drawString(count, 10, 40);
      // Serial.println("");

      delay(50);
    }

    
    // And just stop, now that we've tried a download
    // while (1)
    //   ;
    
    // delay(2000);
    
}