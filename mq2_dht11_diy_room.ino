#include <WiFi.h> 
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "*********"
#define WIFI_PASSWORD "***********"

/* 2. Define the API Key */
#define API_KEY "**************"

/* 3. Define the RTDB URL */
#define DATABASE_URL "******************" 

/* 4. Define the user Email and password that already registered or added in your project */
#define USER_EMAIL "*********************"
#define USER_PASSWORD "**********"

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

const int gasPin = 36;
const int irLedPin = 19;  // IR LED for window control
const int relayPin = 14;  // Relay for humidity control
DHT dht(4, DHT11);

bool is_manual = false;
bool manual_state = false;

void setup() {
  pinMode(gasPin, INPUT);
  pinMode(irLedPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  
  digitalWrite(relayPin, LOW); // Initial state of relay
  
  dht.begin();

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.reconnectNetwork(true);

  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
}

void loop() {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 200 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Check manual control status
    if (Firebase.RTDB.getBool(&fbdo, F("/house/is_manual"), &is_manual)) {
      Serial.print("Manual mode: ");
      Serial.println(is_manual);
    }

    if (is_manual) {
      // Manual control
      if (Firebase.RTDB.getBool(&fbdo, F("/house/manual"), &manual_state)) {
        digitalWrite(relayPin, manual_state ? HIGH : LOW);
        Serial.print("Manual relay state: ");
        Serial.println(manual_state ? "ON" : "OFF");
      }
    } else {
      // Automatic mode
      // Read raw analog value
      int raw_adc = analogRead(gasPin);
      Serial.print("Raw analog value: ");
      Serial.println(raw_adc);

      // Send raw analog value to Firebase
      Firebase.RTDB.setInt(&fbdo, F("/house/raw_value"), raw_adc);

      // Condition to control IR LED (window)
      if (raw_adc > 700) { // Example threshold
        digitalWrite(irLedPin, HIGH); // Open window
        Serial.println("window ON");
      } else if (raw_adc < 350) {
        digitalWrite(irLedPin, LOW); // Close window
        Serial.println("window OFF");
      }

      delay(100);

      int h = dht.readHumidity();
      int t = dht.readTemperature();

      if (isnan(h) || isnan(t)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
      }

      // Send temperature and humidity to Firebase
      Firebase.RTDB.setInt(&fbdo, F("/house/temp"), t);
      Firebase.RTDB.setInt(&fbdo, F("/house/humidity"), h);

      // Condition to control relay (humidity)
      if (h <= 34) {
        digitalWrite(relayPin, HIGH); // Turn on relay
        Serial.println("Relay ON");
        delay(40);
      } else if (h >= 37) {
        digitalWrite(relayPin, LOW); // Turn off relay
        Serial.println("Relay OFF");
        delay(40);
      }
    }
  }
}
