//Include library yang digunakan
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>
#include "DHT.h"

#define DHTPIN 13 
#define DHTTYPE DHT22  // Using DHT22
DHT dht(DHTPIN, DHTTYPE);

//Autentikasi wifi dan firebase
#define WIFI_SSID "."
#define WIFI_PASSWORD "123456789s"
#define API_KEY "AIzaSyAhPs5IIFZWDV_NuLGNiz1aGyivROkJqFg"
#define DATABASE_URL "https://room-temperature-m-onitoring-default-rtdb.asia-southeast1.firebasedatabase.app/"

// NTP Server untuk mendapatkan waktu
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 25200  // GMT+7 Jakarta (7 * 60 * 60)
#define DAYLIGHT_OFFSET_SEC 0

//inisiasi objek dari firebase library
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
float humidityData = 0;
float temperatureData = 0;

void setup() {
  Serial.begin(115200);

  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED){
      Serial.print("."); delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Inisialisasi NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("Syncing time with NTP server...");
  delay(2000);
  
  // Check time sync
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synchronized");
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    Serial.println(timeStringBuff);
  }

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("signUp OK");
    signupOK = true;
  }else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    
    // Read data from DHT22 sensor
    humidityData = dht.readHumidity();
    temperatureData = dht.readTemperature();
    
    // Check if reading failed
    if (isnan(humidityData) || isnan(temperatureData)) {
      Serial.println("Failed to read from DHT22 sensor!");
      return;
    }

    // Get current timestamp
    struct tm timeinfo;
    char timestamp[30];
    
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      strcpy(timestamp, "Time not available");
    } else {
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    
    // Create JSON object with sensor data and timestamp
    json.clear();
    json.set("humidity", humidityData);
    json.set("temperature", temperatureData);
    json.set("timestamp", timestamp);
    
    // Generate a unique key based on timestamp (for push-style storage)
    char dataPath[100];
    sprintf(dataPath, "/sensor_readings/%d", (int)time(NULL));
    
    // Send JSON object to Firebase
    if(Firebase.RTDB.setJSON(&fbdo, dataPath, &json)){
      Serial.println("Successfully saved reading with timestamp:");
      Serial.print("Temperature: "); Serial.print(temperatureData); Serial.println(" °C");
      Serial.print("Humidity: "); Serial.print(humidityData); Serial.println(" %");
      Serial.print("Time: "); Serial.println(timestamp);
      Serial.print("Path: "); Serial.println(fbdo.dataPath());
    } else {
      Serial.println("FAILED: " + fbdo.errorReason());
    }
    
    // Also update the current values separately for easy access
    if(Firebase.RTDB.setFloat(&fbdo, "/current_values/dht22_humidity", humidityData) && 
       Firebase.RTDB.setFloat(&fbdo, "/current_values/dht22_temperature", temperatureData) &&
       Firebase.RTDB.setString(&fbdo, "/current_values/last_update", timestamp)){
      Serial.println("Current values updated");
    } else {
      Serial.println("Failed to update current values: " + fbdo.errorReason());
    }
  }
}