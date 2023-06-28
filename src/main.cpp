#include <SPI.h>
#include <Arduino.h>
#include <MFRC522.h>
#include <freertos/FreeRTOS.h>
#include <freertos/list.h>
#include <WiFiManager.h> 
#include <time.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "freertos/semphr.h"
#include <LiquidCrystal_I2C.h>

//firebase
#define API_KEY "AIzaSyCNJugv3_95Fwq36fm9lmyRuho2sJ8OOM8"
#define DATABASE_URL "https://smartdoorlock-40077-default-rtdb.asia-southeast1.firebasedatabase.app/" 
FirebaseData fbdo;
FirebaseData fbda;
FirebaseJson json;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

//NTP 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600;
const int   daylightOffset_sec = 3600;
char waktu[20];
char waktud[20];

//RFID
#define RST_PIN         15          
#define SS_PIN          5    
MFRC522 mfrc522(SS_PIN, RST_PIN); 
MFRC522::MIFARE_Key key;  
char uid[10];

//LCD
#define I2C_SDA 25
#define I2C_SCL 26
LiquidCrystal_I2C lcd(0x27, 16, 2);

//wifi
#define WIFI_SSID "Chiko"
#define WIFI_PASSWORD "chikojuga"

SemaphoreHandle_t xSemaphore = NULL;

void printHex(byte *buffer, byte bufferSize);
void printLocalTime();

bool stale = true;
int ulang=0;

void baca_kartu(void * parameters){
  for(;;){
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
    if (stale)
    { 
      if (ulang==0)
      {
        lcd.clear();
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        strftime(waktud,20,"%d %b %y, %H:%M:%S", &timeinfo);
        lcd.setCursor(0,1);
        lcd.print(waktud);
        lcd.setCursor(0,0);
        lcd.print("Scan your card!");
        ulang++;
      }
      else if (ulang>20)
      {
        ulang=0;
      } else
      {
        ulang++;
      }
      vTaskDelay(500/portTICK_PERIOD_MS);
    }
	}

	// Select one of the cards
	  if ( ! mfrc522.PICC_ReadCardSerial()) {
	
	} else
  {
    stale=false;
    sprintf(uid, "%d %d %d %d", mfrc522.uid.uidByte[0],mfrc522.uid.uidByte[1],mfrc522.uid.uidByte[2],mfrc522.uid.uidByte[3]);
    
    Serial.print("Uid is:");
    printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
    //lcd section
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Uid is :");
    lcd.setCursor(0,1);
    lcd.print(uid);
    Serial.println();
    Serial.print("Time scanned is:");
    printLocalTime();
    Serial.println();
    xSemaphoreGive(xSemaphore);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    
  }
  
  }
}

void kirim_data(void * parameters){
  for(;;){
    if(xSemaphoreTake(xSemaphore,portMAX_DELAY)){
      json.set("UID",uid );
      json.set("time",waktu);
    if (Firebase.ready() && signupOK ){
    
    if (Firebase.RTDB.pushJSON(&fbdo,"uid/history_log",&json)){
      Serial.println("PASSED");
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    char find[50];
    sprintf(find,"uid/access/%s",uid);
    Serial.println(find);
    
    if (Firebase.RTDB.getJSON(&fbda, find))
    {
      Serial.println(fbda.stringData());
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Granted");
      lcd.setCursor(0,1);
      lcd.print(fbda.stringData());
      digitalWrite(33,LOW);
      vTaskDelay(2000/portTICK_PERIOD_MS);
      digitalWrite(33,HIGH);
      vTaskDelay(3000/portTICK_PERIOD_MS);
      stale=true;
    }
    else
    {
      // Failed to get JSON data at defined database path, print out the error reason
    Serial.println(fbda.errorReason());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Access Not Granted");
    vTaskDelay(3000/portTICK_PERIOD_MS);
    stale=true;
    }
    }
    
  }
  }
}




void setup() {
	Serial.begin(9600);
  pinMode(33,OUTPUT);
  digitalWrite(33,HIGH);		
	while (!Serial);		
	SPI.begin();			
	mfrc522.PCD_Init();		
	delay(4);				
	mfrc522.PCD_DumpVersionToSerial();	
	for (byte i = 0; i < 6; i++) {
    	key.keyByte[i] = 0xFF;
  }
  
  //lcdsetup
  lcd.init(I2C_SDA, I2C_SCL); 
	lcd.backlight();
  

  //wifi
  WiFiManager wm;
  bool done;
  lcd.setCursor(0,0);
  lcd.print("Please Connect :");
  lcd.setCursor(0,1);
  lcd.print("to Smartdoorlock");
  done=wm.autoConnect("Smartdoorlock","password");
  
  
  Serial.println();
  lcd.clear();
  lcd.setCursor(0,0);
  Serial.print("Connected to : ");
  lcd.print("Connected to : ");
  Serial.println(wm.getWiFiSSID());
  lcd.setCursor(0,1);
  lcd.print(wm.getWiFiSSID());
  delay(5000);
  Serial.println();

  // firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  while (!Firebase.signUp(&config, &auth, "", "")){
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
    Firebase.signUp(&config, &auth, "", "");
  }
  Serial.println("ok");
  signupOK = true;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //waktu
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time trying again...");
    configTime(gmtOffset_sec,daylightOffset_sec,ntpServer);
  }
  printLocalTime();

  

  xSemaphore = xSemaphoreCreateBinary();
  
  xTaskCreate(
    baca_kartu, "baca_kartu", 4000,NULL,0,NULL
  );

  xTaskCreate(
    kirim_data, "kirim_data", 8000,NULL,0, NULL
    );

}

void loop() {
}

void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  strftime(waktu,20,"%d %B %Y, %H:%M:%S", &timeinfo);
}

