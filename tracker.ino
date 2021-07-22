#include <Adafruit_FONA.h>

/*
 *
     TEAM IOTRON 
  Findme Device Code 
    NaijaHacks 2019 
 * 
 */
#include "Adafruit_FONA.h"

// standard pins for the shield, adjust as necessary
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4
#define mystatus "RAA status"
#define kill "RAA kill"
#define allow "RAA fkill"
#define Emergency "08103708977"
#define googlemap "https://maps.google.com/maps?q="
uint16_t fixtime = 3000;
String message="";
String stat = "RAA:active\nLoc:";

#define relay 8
#define led 7

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines 
// and uncomment the HardwareSerial line
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// Hardware serial is also possible!
//  HardwareSerial *fonaSerial = &Serial1;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Have a FONA 3G? use this object type instead
//Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
//String TriggerCase = "";
//unsigned long intervals = 0;
float latitude, longitude, speed_kph, heading, altitude;

char fonaNotificationBuffer[64];          //for notifications from the FONA
char smsBuffer[64];
char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer


boolean myLocation(){
   boolean con = false;
   unsigned long previous = millis();
   while(millis() - previous<fixtime){
    boolean gps_success = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
    if(gps_success){
      con=true;
      break;
    }
    if(!con) return con;
    return con;
}
}

void setup() {
  pinMode(relay, OUTPUT);
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  while (! Serial);
  //pinMode(button, INPUT);
  Serial.begin(9600);
  //Serial.println(F("Adafruit FONA 808 & 3G GPS demo"));
  //Serial.println(F("Initializing FONA... (May take a few seconds)"));

  fonaSerial->begin(9600);
  if (! fona.begin(*fonaSerial)) {
    //Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));
  // Try to enable GPRS
  Serial.println(F("Enabling GPS..."));
  fona.enableGPS(true);
  fonaSerial->print("AT+CNMI=2,1\r\n");
  digitalWrite(led, HIGH);

  // Print SIM card IMEI number.
  char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  //fonaSerial->print("AT+CNMI=2,1\r\n");  //set up the FONA to send a +CMTI notification when an SMS is received

  Serial.println("FONA Ready");
}

  


/*bool debounce(int delayTime){
  if (digitalRead(button)){
    delay(delayTime);
    if (digitalRead(button))
      return true;
  }
  return false;
}*/



void loop() {
  char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  uint16_t smslen;
  int slot;
  int charCount;
  char callerIDbuffer[32];  //we'll store the SMS sender number in here
  
  if (fona.available())      //any data available from the FONA?
  {
    slot = 0;            //this will be the slot number of the SMS
    charCount = 0;
    //Read the notification into fonaInBuffer
    do  {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;

    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);

      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) {
        Serial.println("SMS not in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);
     // Retrieve SMS value.
      
      if(fona.readSMS(slot, smsBuffer, 64, &smslen)){
        Serial.println(smsBuffer);
            
        if( strcmp(smsBuffer, mystatus) == 0 ) {
          if(myLocation()){
            message = googlemap + String(latitude,6)+","+String(longitude,6);
            message = stat + message +"\nSpeed:"+ (String)speed_kph + "KPH";
            if(!fona.sendSMS(callerIDbuffer, message.c_str()))
             Serial.println(F("Failed"));
          }
          else{
            if(!fona.sendSMS(callerIDbuffer, "Oops! no fix"))
             Serial.println(F("Failed"));
          }
        }
      }
    }
  }
}
