#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include "HX711.h"
#include <SPI.h>
#include <SD.h>

// LCD 
LiquidCrystal_I2C lcd(0x27, 16, 2);

//  RTC 
RTC_DS3231 rtc;

// DS18B20 
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//LED
#define LED_PIN 3
#define NUM_LEDS 7
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Load Cell (HX711)
#define LOADCELL_DOUT_PIN 6
#define LOADCELL_SCK_PIN 7
HX711 scale;

// Buzzer
#define BUZZER_PIN 9

//SD Card initialization
#define SD_CS_PIN 10
int mealNumber = 1;

//peltier module
#define RELAY_PIN 4

//  Menu Buttons (Analog pins as digital) 
#define BTN_MENU   14  // A0
#define BTN_UP     15  // A1
#define BTN_DOWN   16  // A2
#define BTN_SELECT 17  // A3

// Button state variables 
bool prevMenuBtn = false;
bool prevUpBtn = false;
bool prevDownBtn = false;
bool prevSelectBtn = false;

bool inMenu = false;
int selectedIndex = 0;
unsigned long lastButtonPress = 0;

//  Food timer variables
bool foodTimerActive = false;
DateTime foodAddedTime;
DateTime foodEatTime;
bool alarmTriggered = false;

//  Menu items 
const char* menuItems[] = {"Temperature", "Time", "Weight", "Food Timer", "Heat Food", "Report"};
const int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);

// Helper Functions 
bool isButtonPressed(int pin) {
  return (digitalRead(pin) == LOW);
}

void setLEDColor(float tempC) {
  uint32_t color;
  if (tempC < 25.0)
    color = strip.Color(0, 0, 255);
  else if (tempC < 30.0)
    color = strip.Color(0, 255, 0);
  else
    color = strip.Color(255, 0, 0);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void turnOffLEDs() {
  strip.clear();
  strip.show();
}

void displayMenu() {
  lcd.clear();
  if (selectedIndex <= 1) {
    lcd.setCursor(0, 0);
    lcd.print((selectedIndex == 0) ? "> " : "  ");
    lcd.print(menuItems[0]);

    lcd.setCursor(0, 1);
    lcd.print((selectedIndex == 1) ? "> " : "  ");
    lcd.print(menuItems[1]);
  } else if (selectedIndex == 2) {
    lcd.setCursor(0, 0);
    lcd.print("> ");
    lcd.print(menuItems[2]);

    lcd.setCursor(0, 1);
    lcd.print("  ");
    lcd.print(menuItems[3]);
  } else if (selectedIndex == 3) {
    lcd.setCursor(0, 0);
    lcd.print("> ");
    lcd.print(menuItems[3]);

    lcd.setCursor(0, 1);
    lcd.print("  ");
    lcd.print(menuItems[4]);
  } else if (selectedIndex == 4) {
    lcd.setCursor(0, 0);
    lcd.print("> ");
    lcd.print(menuItems[4]);

    lcd.setCursor(0, 1);
    lcd.print("  ");
    lcd.print(menuItems[5]);   
  } else if (selectedIndex == 5) {
    lcd.setCursor(0, 0);
    lcd.print("> ");
    lcd.print(menuItems[5]);   
  }
}


//  Food Timer Alert 
void foodTimerAlert() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < NUM_LEDS; j++) {
      strip.setPixelColor(j, strip.Color(255, 0, 0));
    }
    strip.show();
    tone(BUZZER_PIN, 2000, 500);
    delay(500);

    strip.clear();
    strip.show();
    delay(500);
  }
}

// Time setter 
int setTimeValue(const char* prompt, int minVal, int maxVal, int currentVal) {
  int value = currentVal;
  bool prevUpBtn = false;
  bool prevDownBtn = false;
  bool prevSelectBtn = false;

  while (true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(prompt);
    lcd.setCursor(0, 1);
    lcd.print("Value: ");
    if (value < 10) lcd.print("0");
    lcd.print(value);
    lcd.print(" (Select=OK)");

    delay(50);

    bool currentUpBtn = isButtonPressed(BTN_UP);
    bool currentDownBtn = isButtonPressed(BTN_DOWN);
    bool currentSelectBtn = isButtonPressed(BTN_SELECT);

    if (currentUpBtn && !prevUpBtn) {
      value++;
      if (value > maxVal) value = minVal;
      tone(BUZZER_PIN, 1000, 100);
      delay(200);
    } else if (currentDownBtn && !prevDownBtn) {
      value--;
      if (value < minVal) value = maxVal;
      tone(BUZZER_PIN, 1000, 100);
      delay(200);
    } else if (currentSelectBtn && !prevSelectBtn) {
      tone(BUZZER_PIN, 1500, 150);
      delay(300);
      return value;
    }

    prevUpBtn = currentUpBtn;
    prevDownBtn = currentDownBtn;
    prevSelectBtn = currentSelectBtn;
  }
}






//  Food Timer Handler 
void handleFoodTimer() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting Food");
  lcd.setCursor(0, 1);
  lcd.print("Timer...");
  delay(1000);

  foodAddedTime = rtc.now();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Food added at:");
  lcd.setCursor(0, 1);
  lcd.print(foodAddedTime.hour() < 10 ? "0" : "");
  lcd.print(foodAddedTime.hour());
  lcd.print(":");
  lcd.print(foodAddedTime.minute() < 10 ? "0" : "");
  lcd.print(foodAddedTime.minute());
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set eat time:");
  delay(1500);

  int eatHour = setTimeValue("Set Hour:", 0, 23, foodAddedTime.hour());
  int eatMinute = setTimeValue("Set Minute:", 0, 59, foodAddedTime.minute());

  foodEatTime = DateTime(foodAddedTime.year(), foodAddedTime.month(), foodAddedTime.day(), eatHour, eatMinute, 0);
  if (foodEatTime.unixtime() <= foodAddedTime.unixtime()) {
    foodEatTime = DateTime(foodEatTime.unixtime() + 86400);
  }

  foodTimerActive = true;
  alarmTriggered = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Eat by: ");
  lcd.print(foodEatTime.hour() < 10 ? "0" : "");
  lcd.print(foodEatTime.hour());
  lcd.print(":");
  lcd.print(foodEatTime.minute() < 10 ? "0" : "");
  lcd.print(foodEatTime.minute());
  delay(3000);

  tone(BUZZER_PIN, 1000, 200);
  delay(200);
  tone(BUZZER_PIN, 1500, 200);
  delay(200);

  prevMenuBtn = false;
  prevUpBtn = false;
  prevDownBtn = false;
  prevSelectBtn = false;
  lastButtonPress = millis();

  inMenu = true;
  displayMenu();
}

// Check Food Timer 
void checkFoodTimer() {
  if (!foodTimerActive || alarmTriggered) return;

  DateTime now = rtc.now();

  if (now.unixtime() >= foodEatTime.unixtime()) {
    alarmTriggered = true;
    foodTimerAlert();
  }
}

//Display Food Timer Status 
void displayFoodTimerStatus() {
  bool prevMenuBtnInView = false;

  while (inMenu) {
    lcd.clear();
    checkFoodTimer();

    if (!foodTimerActive) {
      lcd.setCursor(0, 0);
      lcd.print("No active timer");
      lcd.setCursor(0, 1);
      lcd.print("Press UP to set");
    } else {
      DateTime now = rtc.now();
      long timeLeft = foodEatTime.unixtime() - now.unixtime();

      lcd.setCursor(0, 0);
      if (timeLeft > 0) {
        int hoursLeft = timeLeft / 3600;
        int minutesLeft = (timeLeft % 3600) / 60;
        lcd.print("Eat in ");
        lcd.print(hoursLeft);
        lcd.print("h ");
        lcd.print(minutesLeft);
        lcd.print("m");
      } else {
        lcd.print("TIME TO EAT!");
      }

      lcd.setCursor(0, 1);
      lcd.print("Target: ");
      lcd.print(foodEatTime.hour() < 10 ? "0" : "");
      lcd.print(foodEatTime.hour());
      lcd.print(":");
      lcd.print(foodEatTime.minute() < 10 ? "0" : "");
      lcd.print(foodEatTime.minute());
    }

    bool currentUpBtn = isButtonPressed(BTN_UP);
    bool currentMenuBtn = isButtonPressed(BTN_MENU);

    if (currentUpBtn && !foodTimerActive) {
      handleFoodTimer();
      return;
    }

    if (currentMenuBtn && !prevMenuBtnInView && millis() - lastButtonPress > 300) {
      displayMenu();
      lastButtonPress = millis();
      tone(BUZZER_PIN, 1800, 200);
      return;
    }

    prevMenuBtnInView = currentMenuBtn;
    delay(200);
  }
}

//heating process
void handleHeatFood() {
  int options[] = {3, 5, 7};   // minutes
  int optionIndex = 0;
  bool prevUpBtn = false, prevDownBtn = false, prevSelectBtn = false;
  
  while (true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Heat Food Time:");
    lcd.setCursor(0, 1);
    lcd.print(options[optionIndex]);
    lcd.print(" min  (OK=Start)");

    bool currentUpBtn = isButtonPressed(BTN_UP);
    bool currentDownBtn = isButtonPressed(BTN_DOWN);
    bool currentSelectBtn = isButtonPressed(BTN_SELECT);

    if (currentUpBtn && !prevUpBtn) {
      optionIndex = (optionIndex + 1) % 3;
      tone(BUZZER_PIN, 1000, 100);
      delay(200);
    }
    else if (currentDownBtn && !prevDownBtn) {
      optionIndex = (optionIndex - 1 + 3) % 3;
      tone(BUZZER_PIN, 1000, 100);
      delay(200);
    }
    else if (currentSelectBtn && !prevSelectBtn) {
      tone(BUZZER_PIN, 1500, 200);
      int durationSec = options[optionIndex] * 60;
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Heating for ");
      lcd.print(options[optionIndex]);
      lcd.print(" min");
      
      digitalWrite(RELAY_PIN, HIGH); // Relay ON
      
      unsigned long start = millis();
      while ((millis() - start) / 1000 < durationSec) {
        int remaining = durationSec - (millis() - start) / 1000;
        lcd.setCursor(0, 1);
        lcd.print("Time Left: ");
        lcd.print(remaining);
        lcd.print("s   ");
        delay(500);
      }
      
      digitalWrite(RELAY_PIN, LOW); // Relay OFF
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Heating Done!");
      foodTimerAlert(); // reuse buzzer + LED alert
      delay(3000);
      displayMenu();
      return;
    }

    prevUpBtn = currentUpBtn;
    prevDownBtn = currentDownBtn;
    prevSelectBtn = currentSelectBtn;
    delay(100);
  }
}




void saveReport() {
    // Request temperature
    sensors.requestTemperatures();
    delay(500);
    float tempC = sensors.getTempCByIndex(0);

    // Get weight from load cell
    long weightValue = 0;
    if (scale.is_ready()) weightValue = scale.get_units(5);

    // Safe filename: MEAL01.TXT, MEAL02.TXT, etc.
    char fileName[12];
    sprintf(fileName, "MEAL%02d.TXT", mealNumber);
    mealNumber = (mealNumber % 99) + 1; // increment safely

    Serial.print("Creating file: "); Serial.println(fileName);

    // Open file for writing
    File file = SD.open(fileName, FILE_WRITE);
    if (!file) {
        Serial.println("SD write failed!");
        lcd.clear();
        lcd.print("SD Write Err");
        return;
    }

    // Convert temperature to string
    char tempStr[10];
    dtostrf(tempC, 5, 1, tempStr);

    // Meal Added Time
    file.print("Meal Added Time: ");
    if (foodAddedTime.hour() < 10) file.print("0");
    file.print(foodAddedTime.hour());
    file.print(":");
    if (foodAddedTime.minute() < 10) file.print("0");
    file.println(foodAddedTime.minute());

    // Meal Eaten Time
    file.print("Meal Eaten Time: ");
    if (foodEatTime.hour() < 10) file.print("0");
    file.print(foodEatTime.hour());
    file.print(":");
    if (foodEatTime.minute() < 10) file.print("0");
    file.println(foodEatTime.minute()); 

    file.print("Date: "); file.print(foodAddedTime.year()); file.print("-"); file.print(foodAddedTime.month()); file.print("-"); file.println(foodAddedTime.day());
    file.print("Temperature: "); file.print(tempStr); file.println(" C");
    file.print("Weight: "); file.print(weightValue); file.println(" g");
    file.println();

    file.flush(); // ensure all data is written
    file.close(); // Important to save data

    Serial.println("Report saved!");
    lcd.clear();
    lcd.print("Meal Saved!");
    delay(1000);
}




//  Menu Handler 
void handleMenu() {
  if (millis() - lastButtonPress < 300) return;

  bool currentUpBtn = isButtonPressed(BTN_UP);
  bool currentDownBtn = isButtonPressed(BTN_DOWN);
  bool currentSelectBtn = isButtonPressed(BTN_SELECT);
  bool currentMenuBtn = isButtonPressed(BTN_MENU);

  if (currentUpBtn && !prevUpBtn) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : menuItemCount - 1;
    displayMenu();
    lastButtonPress = millis();
    tone(BUZZER_PIN, 1000, 100);
  } 
  else if (currentDownBtn && !prevDownBtn) {
    selectedIndex = (selectedIndex < menuItemCount - 1) ? selectedIndex + 1 : 0;
    displayMenu();
    lastButtonPress = millis();
    tone(BUZZER_PIN, 1000, 100);
  }
  else if (currentSelectBtn && !prevSelectBtn) {
    lcd.clear();
    tone(BUZZER_PIN, 1500, 150);
    lastButtonPress = millis();

    if (strcmp(menuItems[selectedIndex], "Report") == 0) {
      saveReport();
      displayMenu();
    }
    else if (strcmp(menuItems[selectedIndex], "Heat Food") == 0) {
      handleHeatFood();
    }
    else if (strcmp(menuItems[selectedIndex], "Food Timer") == 0) {
      handleFoodTimer();
    }
    else if (strcmp(menuItems[selectedIndex], "Temperature") == 0) {
      sensors.requestTemperatures();
      float t = sensors.getTempCByIndex(0);
      lcd.clear();
      lcd.print("Temp: "); lcd.print(t); lcd.print(" C");
      delay(2000);
      displayMenu();
    }
    else if (strcmp(menuItems[selectedIndex], "Weight") == 0) {
      lcd.clear();
      lcd.print("Weight: ");
      if (scale.is_ready()) lcd.print(scale.get_units(5));
      else lcd.print("ERR");
      lcd.print(" g");
      delay(2000);
      displayMenu();
    }
    else if (strcmp(menuItems[selectedIndex], "Time") == 0) {
      DateTime now = rtc.now();
      lcd.clear();
      lcd.print("Time: ");
      lcd.print(now.hour()); lcd.print(":"); lcd.print(now.minute());
      lcd.setCursor(0,1);
      lcd.print(now.day()); lcd.print("/");
      lcd.print(now.month()); lcd.print("/");
      lcd.print(now.year());
      delay(2000);
      displayMenu();
    }
  }

  
  prevUpBtn = currentUpBtn;
  prevDownBtn = currentDownBtn;
  prevSelectBtn = currentSelectBtn;
  prevMenuBtn = currentMenuBtn;
}




// Setup 
void setup() {
  Serial.begin(9600);         // Start serial monitor
  lcd.begin();                // Initialize LCD
  lcd.backlight();            // Turn on backlight

  // Buttons 
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);  // Buzzer output
  pinMode(RELAY_PIN, OUTPUT);   // Peltier relay
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  // NeoPixel 
  strip.begin();
  strip.show();  // Initialize all pixels to off

  // RTC 
  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("RTC failed");
    Serial.println("RTC failed");
    while (1); // Halt execution if RTC fails
  }

  //  SD Card 
  pinMode(SD_CS_PIN, OUTPUT);
  if (!SD.begin(SD_CS_PIN)) {
    lcd.clear();
    lcd.print("SD init failed!");
    Serial.println("SD init failed!");
    while (1); // Stop execution if SD fails
  }

  // Load Cell 
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale();
  scale.tare();

  // Temperature Sensor 
  sensors.begin();

  // Start menu 
  inMenu = true;
  displayMenu();
}

// Main Loop 
void loop() {
  if (inMenu) handleMenu();
  
  checkFoodTimer();
}



