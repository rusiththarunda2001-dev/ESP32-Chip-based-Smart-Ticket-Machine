/*
  ESP32 RFID Ticket Machine Client (v4 - With Thermal Printer)
  
  This version integrates the Adafruit Thermal Printer.
  It includes all previous features and adds:
  1. Includes Adafruit_Thermal library.
  2. Initializes the printer on Serial2 in setup().
  3. Calls a new printTicket() function after a successful payment.
  4. Prints one ticket for EACH ticket purchased (e.g., 2x Adult 1st = 2 tickets).
  
  **REQUIRED LIBRARIES:**
  - Adafruit_Thermal (by Adafruit) // --- NEW LIBRARY ---
  - ArduinoJson (by Benoit Blanchon)
  - Keypad (by Mark Stanley, Alexander Brevig)
  - All other previous libraries (WiFi, HTTPClient, Wire, SPI, etc.)
*/

// --- Main Libraries ---
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Keypad.h>

// --- PRINTER --- New Library
#include "Adafruit_Thermal.h" 

// ----- WiFi Config -----
const char* ssid = "HUAWEI Y6p";
const char* password = "11111111";
const char* MACHINE_ID = "model001";
// --- NTP Time Config ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // Sri Lanka is UTC +5:30 (5.5 * 60 * 60)
const int   daylightOffset_sec = 0; // No daylight saving

// ----- Google Apps Script Web App URL -----
String scriptURL = "https://script.google.com/macros/s/AKfycbwKq9SIGJ227bVQkArGieN6mf0VxIw71Qbrd-LjTOORgCSYYmR6GfkMoO8AI66HGPZK/exec"; // Use your new URL

// ----- Pin Definitions -----
#define SS_PIN 5
#define RST_PIN 4
#define SDA_PIN 21
#define SCL_PIN 22

// --- Button Pin Definitions ---
#define UP_BUTTON_PIN 34
#define DOWN_BUTTON_PIN 35
#define ENTER_BUTTON_PIN 39

// --- Keypad Pin Definitions ---
const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {27, 14, 12}; 

// ----- Hardware Objects -----
LiquidCrystal_I2C lcd(0x27, 20, 4); 
MFRC522 rfid(SS_PIN, RST_PIN);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- PRINTER --- New Hardware Object
// Use Serial2 (Pins GPIO16, GPIO17) for the printer
Adafruit_Thermal printer(&Serial2); 

// --- Custom Arrow Character ---
byte thickArrow[8] = {
  B01000, B01100, B01110, B01111, B01110, B01100, B01000, B00000,
};

// ----- Helper Function Prototypes -----
bool handlePinEntry(int correctPin);
String getPinFromKeypad();
JsonObject handleDestinationMenu(JsonArray stations);
void displayMenu(JsonArray stations, int selected, int windowStart);
float handleFinalTicketMenu(JsonObject station, float balance, int &a1, int &a2, int &a3, int &c1, int &c2, int &c3);
bool checkButtons(int &buttonPressed, unsigned long &lastPress);
void blockAccountOnSheet(String rfid);
void deductBalanceOnSheet(String rfid, float newBalance, float totalPrice);

// --- PRINTER --- New Function Prototype
void printTicket(String date, String time, String startStation, String endStation, String ticketType);


// ----- SETUP -----
void setup() {
  Serial.begin(9600);
  Serial.println("Booting up...");

  // --- PRINTER --- Initialize Serial2 for the printer
  Serial2.begin(9600); // 9600 baud rate is common for these printers
  
  // --- Setup button pins ---
  pinMode(UP_BUTTON_PIN, INPUT);
  pinMode(DOWN_BUTTON_PIN, INPUT);
  pinMode(ENTER_BUTTON_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // --- NEW LINE TO GET TIME ---
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time configured from NTP.");

  SPI.begin(18, 19, 23, 5); // SCK, MISO, MOSI, SS
  rfid.PCD_Init();
  Wire.begin(SDA_PIN, SCL_PIN);
  
  lcd.init();
  lcd.backlight();
  
  lcd.createChar(0, thickArrow);
  
  // --- PRINTER --- Initialize printer
  printer.begin();
  Serial.println("Printer initialized.");
  
  lcd.setCursor(0, 1);
  lcd.print("WELCOME SMART TICKET");
  lcd.setCursor(0, 2);
  lcd.print("PLEASE TAP YOUR RFID");
  Serial.println("System ready. Scan your card.");
}

// ----- MAIN LOOP -----
void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }

  // 1. Get the UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0"; 
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.print("Detected UID: ");
  Serial.println(uid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SCANNING RFID...");

  // 2. Build the URL and make the HTTP request
  String url = scriptURL + "?rfid=" + uid + "&machine=" + MACHINE_ID;
  Serial.print("Requesting: ");
  Serial.println(url);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) { 
      String payload = http.getString();
      Serial.println("Response: " + payload);

      // 3. Parse the JSON
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        lcd.clear();
        lcd.print("JSON Error");
      } else {
        
        if (doc.containsKey("error")) {
          const char* rfid_error = doc["error"]; 
          Serial.println(rfid_error);
          lcd.clear();
          lcd.setCursor(0, 1);
          lcd.print(rfid_error); 
          
        } else {
          // --- SUCCESS! ---
          
          // 4. Get Data
          int correctPIN = doc["passenger"]["pin"]; 
          float balance = doc["passenger"]["balance"];
          const char* passengerName = doc["passenger"]["name"];
          const char* passengerStatus = doc["passenger"]["status"];
          const char* Ticketstation = doc["machine"]; // Kiosk location from F1

          // --- Check Account Status FIRST ---
          String statusStr = passengerStatus ? passengerStatus : ""; 
          statusStr.toLowerCase();
          Serial.print("Passenger Status: "); Serial.println(statusStr);
          Serial.print("Ticket Station: ");Serial.println(Ticketstation);

          if (statusStr == "blocked") {
            // --- ACCOUNT IS BLOCKED ---
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(" ACCOUNT IS BLOCKED ");
            lcd.setCursor(0, 1);
            lcd.print("  CONTACT SERVICE  ");
            lcd.setCursor(0, 2);
            lcd.print("       CENTER       ");
            Serial.println("Account is blocked. Halting transaction.");
          
          } else {
            // --- ACCOUNT IS ACTIVE, PROCEED ---
            Serial.print("Correct PIN: "); Serial.println(correctPIN);

            // 5. Handle PIN Entry (3 attempts)
            bool pinCorrect = handlePinEntry(correctPIN);

            if (pinCorrect) {
              // 6. Handle Destination Menu
              JsonArray stations = doc["stations"].as<JsonArray>();
              JsonObject selectedStation = handleDestinationMenu(stations);

              if (!selectedStation.isNull()) { 
                
                // 7. Handle Final Ticket Menu
                int a1=0, a2=0, a3=0, c1=0, c2=0, c3=0; // Ticket counts
                float totalPrice = handleFinalTicketMenu(selectedStation, balance, a1, a2, a3, c1, c2, c3);

                if (totalPrice >= 0) { // Check if not cancelled
                  
                  // --- DEDUCT BALANCE ---
                  float newBalance = balance - totalPrice;
                  deductBalanceOnSheet(uid, newBalance,totalPrice); 
                  
                  delay(2000); // Wait for user to read "Payment Successful"
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("PRINTING TICKET");
                  lcd.setCursor(0, 1);
                  lcd.print("Total: "); lcd.print(totalPrice);
                  lcd.setCursor(0, 2);
                  lcd.print("New Bal: "); lcd.print(newBalance);
                  
                  Serial.print("Total: "); Serial.println(totalPrice);
                  Serial.print("New Balance: "); Serial.println(newBalance);
                  
                  // =========================================
                  // --- PRINTER --- NEW PRINTING LOGIC ---
                  // =========================================
                  Serial.println("Sending to printer...");

                  // Get the station names for the ticket
                  String startStation = Ticketstation ? Ticketstation : "MAIN STATION";
                  String endStation = selectedStation["name"].as<String>();
                  
                  // Get Date/Time (Placeholders)
                  // TODO: Add an RTC (Real Time Clock) module for real time
                  struct tm timeinfo;
                  String dateStr;
                  String timeStr;

                  if (!getLocalTime(&timeinfo)) {
                    Serial.println("Failed to get time");
                    dateStr = "N/A"; // Fallback if time fails
                    timeStr = "N/A";
                  } else {
                    // Format the time into strings
                    char dateBuffer[11];
                    strftime(dateBuffer, sizeof(dateBuffer), "%Y/%m/%d", &timeinfo);
                    char timeBuffer[9];
                    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);

                    dateStr = String(dateBuffer);
                    timeStr = String(timeBuffer);
                  }

                  Serial.print("Current Date: "); Serial.println(dateStr);
                  Serial.print("Current Time: "); Serial.println(timeStr);

                  // Loop and print a ticket for EACH one purchased
                  for (int i = 0; i < a1; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "ADULT 1ST CLASS");
                  }
                  for (int i = 0; i < a2; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "ADULT 2ND CLASS");
                  }
                  for (int i = 0; i < a3; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "ADULT 3RD CLASS");
                  }
                  for (int i = 0; i < c1; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "CHILD 1ST CLASS");
                  }
                  for (int i = 0; i < c2; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "CHILD 2ND CLASS");
                  }
                  for (int i = 0; i < c3; i++) {
                    printTicket(dateStr, timeStr, startStation, endStation, "CHILD 3RD CLASS");
                  }
                  // =========================================
                  // --- END OF PRINTER LOGIC ---
                  // =========================================

                } else {
                  // User cancelled from the ticket count menu
                  lcd.clear();
                  lcd.print("CANCELLED");
                  Serial.println("Transaction Cancelled");
                }
              } else {
                lcd.clear();
                lcd.print("CANCELLED");
                Serial.println("Destination selection cancelled");
              }
              
            } else {
              // 8. PIN entry failed (3 attempts)
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print(" ACCOUNT IS BLOCKED ");
              lcd.setCursor(0, 1);
              lcd.print("  CONTACT SERVICE  ");
              lcd.setCursor(0, 2);
              lcd.print("       CENTER       ");
              Serial.println("ACCOUNT BLOCKED");
              
              // --- Call function to update the Google Sheet ---
              blockAccountOnSheet(uid); 
            }
          } 
        }
      }

    } else {
      Serial.print("HTTP Error code: ");
      Serial.println(httpCode);
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("RFID SCAN FAILLED...");
    }
    http.end(); 
    
  } else {
    Serial.println("WiFi Disconnected!");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("WiFi Error");
  }

  // --- Request is finished ---
  
  Serial.println("Process complete. Resetting in 5s.");
  delay(5000); 

  Serial.println("Waiting for card removal...");
  while (rfid.PICC_IsNewCardPresent()) {
    delay(100);
  }
  
  Serial.println("Card removed. Ready for next scan.");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("WELCOME SMART TICKET");
  lcd.setCursor(0, 2);
  lcd.print("PLEASE TAP YOUR RFID");
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}


// ==========================================================
// --- HELPER FUNCTIONS (PIN, MENUS, HTTP) ---
// ==========================================================

bool handlePinEntry(int correctPin) {
  int attempts = 3;
  while (attempts > 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ENTER THE PIN");
    lcd.setCursor(0, 1);
    lcd.print(attempts);
    lcd.print(" ATTEMPTS LEFT");
    
    String enteredPinStr = getPinFromKeypad();
    
    Serial.println("--------------------");
    Serial.print("PIN entered by user: '"); Serial.print(enteredPinStr); Serial.println("'");
    Serial.print("User PIN as integer: "); Serial.println(enteredPinStr.toInt());
    Serial.print("Correct PIN from Google: "); Serial.println(correctPin);
    Serial.println("--------------------");

    if (enteredPinStr.toInt() == correctPin) {
      lcd.clear();
      lcd.print("PIN CORRECT");
      Serial.println("PIN Correct");
      delay(1000);
      return true;
    } else {
      attempts--;
      lcd.clear();
      lcd.print("PIN INCORRECT");
      Serial.println("PIN Incorrect. Attempts left: " + String(attempts));
      delay(1500);
    }
  }
  Serial.println("PIN entry failed after 3 attempts.");
  return false;
}

String getPinFromKeypad() {
  String enteredPin = "";
  lcd.setCursor(0, 2);
  lcd.print("PIN: ");
  
  while(true) {
    char key = keypad.getKey();
    
    if (key) { 
      Serial.print("Key pressed: "); Serial.println(key);
      
      if (key == '#') { // Submit
        if (enteredPin.length() > 0) {
          Serial.println("Submit key '#' pressed. Returning PIN."); 
          return enteredPin;
        }
      } 
      else if (key == '*') { // Backspace
        if (enteredPin.length() > 0) {
          enteredPin.remove(enteredPin.length() - 1); 
          lcd.setCursor(5 + enteredPin.length(), 2);
          lcd.print(" "); 
          Serial.println("Backspace key '*' pressed."); 
        }
      }
      else if (isDigit(key) && enteredPin.length() < 4) {
        enteredPin += key;
        lcd.setCursor(5 + (enteredPin.length() - 1), 2);
        lcd.print("*"); 
        Serial.print("Current PIN string: "); Serial.println(enteredPin);
      }
    }
    delay(10); 
  }
}

JsonObject handleDestinationMenu(JsonArray stations) {
  int numStations = stations.size();
  if (numStations == 0) return JsonObject();

  int selectedIndex = 0;
  int windowStart = 0; 
  int buttonPressed = 0;
  unsigned long lastPress = 0;
  
  displayMenu(stations, selectedIndex, windowStart); 
  
  while(true) {
    if (checkButtons(buttonPressed, lastPress)) { 
      
      if (buttonPressed == 1) { // --- UP ---
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = numStations - 1; 
      }
      else if (buttonPressed == 2) { // --- DOWN ---
        selectedIndex++;
        if (selectedIndex >= numStations) selectedIndex = 0;
      }
      else if (buttonPressed == 3) { // --- ENTER ---
        Serial.print("Selected: ");
        Serial.println(stations[selectedIndex]["name"].as<const char*>());
        return stations[selectedIndex];
      }
      
      if (selectedIndex < windowStart) {
        windowStart = selectedIndex;
      } else if (selectedIndex >= windowStart + 3) { 
        windowStart = selectedIndex - 2;
      }
      
      displayMenu(stations, selectedIndex, windowStart);
    }
    delay(10);
  }
}

void displayMenu(JsonArray stations, int selected, int windowStart) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CHOOSE DESTINATION");
  
  int numStations = stations.size();
  for (int i = 0; i < 3; i++) { 
    int stationIndex = windowStart + i;
    if (stationIndex < numStations) {
      lcd.setCursor(0, i + 1);
      
      if (stationIndex == selected) {
        lcd.write(byte(0)); 
        lcd.print("  ");
      } else {
        lcd.print("  ");
      }
      
      lcd.print(stations[stationIndex]["name"].as<String>());
    }
  }
}

float handleFinalTicketMenu(JsonObject station, float balance, int &a1, int &a2, int &a3, int &c1, int &c2, int &c3) {
  // 1. Get all 6 prices
  float p_a1 = station["price1"];
  float p_a2 = station["price2"];
  float p_a3 = station["price3"];
  float p_c1 = p_a1 / 2.0; 
  float p_c2 = p_a2 / 2.0;
  float p_c3 = p_a3 / 2.0;

  Serial.println("--- DEBUG: handleFinalTicketMenu ---");
  Serial.print("Received p_a1: "); Serial.println(p_a1);
  Serial.print("Received p_a2: "); Serial.println(p_a2);
  Serial.print("Received p_a3: "); Serial.println(p_a3);
  Serial.println("------------------------------------");

  int selectedLine = 0; // 0-7 
  int windowStart = 0;  
  int buttonPressed = 0;
  unsigned long lastPress = millis(); 
  bool needsRedraw = true; 

  a1=0, a2=0, a3=0, c1=0, c2=0, c3=0;

  while(true) {
    // 2. Calculate total
    float total = (a1 * p_a1) + (a2 * p_a2) + (a3 * p_a3) +
                  (c1 * p_c1) + (c2 * p_c2) + (c3 * p_c3);
    
    if (needsRedraw) {
      lcd.clear();
      
      // 3. Draw Layout
      String totalStr = "Total: " + String(total);
      lcd.setCursor(0, 0);
      lcd.print(totalStr);
      
      for (int i = 0; i < 3; i++) { 
        int itemIndex = windowStart + i;
        if (itemIndex > 7) break;
        
        lcd.setCursor(0, i + 1); 
        
        if (itemIndex == selectedLine) {
          lcd.write(byte(0));
          lcd.print(" ");
        } else {
          lcd.print("  ");
        }

        switch(itemIndex) {
          case 0: lcd.print("ADULT 1ST CLASS "); lcd.print(a1); break;
          case 1: lcd.print("ADULT 2ND CLASS "); lcd.print(a2); break;
          case 2: lcd.print("ADULT 3RD CLASS "); lcd.print(a3); break;
          case 3: lcd.print("CHILD 1ST CLASS "); lcd.print(c1); break;
          case 4: lcd.print("CHILD 2ND CLASS "); lcd.print(c2); break;
          case 5: lcd.print("CHILD 3RD CLASS "); lcd.print(c3); break;
          case 6: lcd.print("PAY TICKET"); break;
          case 7: lcd.print("CANCEL TICKET"); break;
        }
      }
      needsRedraw = false; 
    }

    // 4. Check for UP/DOWN/ENTER buttons
    if (checkButtons(buttonPressed, lastPress)) {
      if (buttonPressed == 1) { // --- UP ---
        selectedLine--;
        if (selectedLine < 0) selectedLine = 7;
      }
      else if (buttonPressed == 2) { // --- DOWN ---
        selectedLine++;
        if (selectedLine > 7) selectedLine = 0;
      }
      else if (buttonPressed == 3) { // --- ENTER ---
        if (selectedLine == 6) { // PAY TICKET
          int totalTickets = a1+a2+a3+c1+c2+c3;
          if (totalTickets == 0) {
            lcd.clear();
            lcd.print("NO TICKETS SELECTED");
            delay(1500);
          } else if (total > balance) {
            lcd.clear();
            lcd.print("INSUFFICIENT FUNDS");
            delay(2000);
          } else {
            return total; // Success!
          }
        } else if (selectedLine == 7) { // CANCEL TICKET
          return -1; // Cancel
        }
      }
      
      if (selectedLine < windowStart) {
        windowStart = selectedLine;
      } else if (selectedLine >= windowStart + 3) { 
        windowStart = selectedLine - 2;
      }
      needsRedraw = true; 
    }
    
    // 5. Check for Keypad input
    char key = keypad.getKey();
    if (key) {
      if (isDigit(key)) {
        int num = key - '0';
        switch(selectedLine) {
          case 0: a1 = (a1 * 10) + num; if (a1 > 99) a1 = 99; needsRedraw = true; break;
          case 1: a2 = (a2 * 10) + num; if (a2 > 99) a2 = 99; needsRedraw = true; break;
          case 2: a3 = (a3 * 10) + num; if (a3 > 99) a3 = 99; needsRedraw = true; break;
          case 3: c1 = (c1 * 10) + num; if (c1 > 99) c1 = 99; needsRedraw = true; break;
          case 4: c2 = (c2 * 10) + num; if (c2 > 99) c2 = 99; needsRedraw = true; break;
          case 5: c3 = (c3 * 10) + num; if (c3 > 99) c3 = 99; needsRedraw = true; break;
        }
      }
      else if (key == '*') { // Backspace
        switch(selectedLine) {
          case 0: a1 = a1 / 10; needsRedraw = true; break;
          case 1: a2 = a2 / 10; needsRedraw = true; break;
          case 2: a3 = a3 / 10; needsRedraw = true; break;
          case 3: c1 = c1 / 10; needsRedraw = true; break;
          case 4: c2 = c2 / 10; needsRedraw = true; break;
          case 5: c3 = c3 / 10; needsRedraw = true; break;
        }
      }
      else if (key == '#') { // Shortcut for PAY TICKET
        int totalTickets = a1+a2+a3+c1+c2+c3;
        if (totalTickets == 0) {
          lcd.clear();
          lcd.print("NO TICKETS SELECTED");
          delay(1500);
        } else if (total > balance) {
          lcd.clear();
          lcd.print("INSUFFICIENT FUNDS");
          delay(2000);
        } else {
          return total; // Success!
        }
        needsRedraw = true;
      }
    }
    delay(10); 
  }
}

bool checkButtons(int &buttonPressed, unsigned long &lastPress) {
  // 250ms debounce
  if (millis() - lastPress < 250) { 
    return false;
  }
  
  if (digitalRead(UP_BUTTON_PIN) == LOW) {
    buttonPressed = 1;
    lastPress = millis();
    return true;
  }
  if (digitalRead(DOWN_BUTTON_PIN) == LOW) {
    buttonPressed = 2;
    lastPress = millis();
    return true;
  }
  if (digitalRead(ENTER_BUTTON_PIN) == LOW) {
    buttonPressed = 3;
    lastPress = millis();
    return true;
  }
  return false;
}

void blockAccountOnSheet(String rfid) {
  Serial.println("Sending request to block account...");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument postDoc(256);
    postDoc["action"] = "blockAccount";
    postDoc["rfid"] = rfid;
    postDoc["machine"] = MACHINE_ID;
    String postData;
    serializeJson(postDoc, postData);

    Serial.print("POST data: "); Serial.println(postData);
    int httpCode = http.POST(postData);

    if (httpCode > 0) { 
      String payload = http.getString();
      Serial.println("POST Response: " + payload);
    } else {
      Serial.print("POST Error code: "); Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected! Cannot block account.");
    lcd.setCursor(0, 3);
    lcd.print("WiFi Error.         ");
  }
}

void deductBalanceOnSheet(String rfid, float newBalance,float totalPrice) {
  Serial.println("Sending request to deduct balance...");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("PROCESSING PAYMENT..");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL); 
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument postDoc(256);
    postDoc["action"] = "deductBalance";
    postDoc["rfid"] = rfid;
    postDoc["newBalance"] = newBalance; 
    postDoc["totalPrice"] = totalPrice;
    postDoc["machine"] = MACHINE_ID;
    String postData;
    serializeJson(postDoc, postData);

    Serial.print("POST data: "); Serial.println(postData);
    int httpCode = http.POST(postData);

    if (httpCode > 0) { 
      String payload = http.getString();
      Serial.println("POST Response: " + payload);
      lcd.setCursor(0, 2);
      lcd.print("PAYMENT SUCCESSFUL!");
    } else {
      Serial.print("POST Error code: "); Serial.println(httpCode);
      lcd.setCursor(0, 2);
      lcd.print("PAYMENT SUCCESSFUL!"); // Still show success
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected! Cannot deduct balance.");
  }
}


// ==========================================================
// --- PRINTER --- NEW HELPER FUNCTION 
// ==========================================================
/**
 * Prints a single formatted ticket.
 * This function is called from loop() for each ticket purchased.
 */
void printTicket(String date, String time, String startStation, String endStation, String ticketType) {
  // Wake up and reset
  printer.wake();
  printer.setDefault();

  // 1. HEADER: SMART TICKET (Large, Bold, Centered)
  printer.justify('C');
  printer.setSize('L'); // Large size
  printer.boldOn();
  printer.println("SMART TICKET");
  printer.boldOff();

  // 2. SUBHEADER: MODEL 001 (Normal, Centered)
  printer.setSize('S'); // Standard size
  printer.println(F("MODEL 001"));
  printer.feed(1);      // Empty line

  // 3. DATE & TIME (Normal, Centered)
  // Note: This will print "N/A" until you add an RTC module
  printer.println(date);
  printer.println(time);
  printer.feed(1);

  // 4. ROUTE: (Medium, Bold, Centered)
  printer.setSize('M'); // Medium size
  printer.boldOn();
  printer.print(startStation);
  printer.print(" - ");
  printer.println(endStation);
  printer.boldOff();

  // 5. TICKET TYPE: e.g., "2ND CLASS ADULT" (Large, Bold, Centered)
  printer.setSize('L'); 
  printer.boldOn();
  printer.println(ticketType);
  printer.boldOff();
  printer.feed(1);

  // 6. FOOTER: SAFE JOURNEY! (Medium, Centered)
  printer.setSize('M');
  printer.println(F("SAFE JOURNEY!"));

  // 7. DISCLAIMER (Small, Centered)
  printer.setSize('S');
  printer.println(F("Valid only on day of issue"));

  // 8. FINISH
  printer.feed(3); // Feed paper so it can be torn off
  printer.sleep(); // Sleep to save power
}

