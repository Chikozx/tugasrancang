#include <SPI.h>
#include <Arduino.h>
#include <MFRC522.h>
#include <freertos/FreeRTOS.h>
#include <freertos/list.h>
#include <WiFi.h>
#include <time.h>
#include <FirebaseESP32.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "freertos/semphr.h"
#include <LiquidCrystal_I2C.h>

//firebase
#define API_KEY "AIzaSyD41dRa3a-6o1f8x8WJzeb4Y0j_0AmSrJQ"
#define DATABASE_URL "https://askohi-default-rtdb.asia-southeast1.firebasedatabase.app/" 
FirebaseData fbdo;
FirebaseJson json;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

//NTP 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
char waktu[20];

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
#define WIFI_SSID "yoseph"
#define WIFI_PASSWORD ""

SemaphoreHandle_t xSemaphore = NULL;

void printHex(byte *buffer, byte bufferSize);
void printLocalTime();

void baca_kartu(void * parameters){
  for(;;){
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
		
	}

	// Select one of the cards
	  if ( ! mfrc522.PICC_ReadCardSerial()) {
	
	} else
  {
    
    sprintf(uid, "%d %d %d %d", mfrc522.uid.uidByte[0],mfrc522.uid.uidByte[1],mfrc522.uid.uidByte[2],mfrc522.uid.uidByte[3]);
    
    Serial.print("Uid is:");
    printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
    //lcd section
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Uid is :");
    lcd.setCursor(0,1);
    lcd.print(uid);
    digitalWrite(33,LOW);
    vTaskDelay(2000);
    digitalWrite(33,HIGH);
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

    }
    // Write an Float number on the database path test/float
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
	Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  //lcdsetup
  lcd.init(I2C_SDA, I2C_SCL); 
	lcd.backlight();
  

  //wifi
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  int count=0;
  // while (WiFi.status() != WL_CONNECTED){
  //   lcd.setCursor(18,0);
	//   lcd.print("Hello, world!");
  //   lcd.setCursor(15,1);
  //   lcd.print("Connecting to Wi-Fi");
  //   delay(500);
  //   lcd.scrollDisplayLeft();
  //   count=count+1;
  //   if (count>=30)
  //   {
  //     lcd.noAutoscroll();
  //     delay(3000);
  //     lcd.clear();
  //     count=0;
  //   }
    
  // }

  Serial.println();
  lcd.clear();
  
  lcd.setCursor(0,0);
  Serial.print("Connected with IP: ");
  lcd.print("Connected with IP: ");
  // Serial.println(WiFi.localIP());
  lcd.setCursor(0,1);
  lcd.print("192.168.1.0");
  delay(5000);
  Serial.println();

  // firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //waktu
  configTime(gmtOffset_sec,daylightOffset_sec,ntpServer);
  printLocalTime();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan your card");
  lcd.setCursor(0,1);
  lcd.print("To open the door!!");

  xSemaphore = xSemaphoreCreateBinary();
  
  xTaskCreate(
    baca_kartu, "baca_kartu", 4000,NULL,0,NULL
  );

  xTaskCreate(
    kirim_data, "kirim_data", 8000,NULL,0,NULL
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

