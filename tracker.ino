#include <Adafruit_FONA.h>
#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>

// standard pins for the shield, adjust as necessary
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

#define mystatus "RAA status"
#define kill "RAA kill"
#define allow "RAA fkill"

#define Emergency "08103708977"
#define googlemap "https://maps.google.com/maps?q="

uint16_t fixtime = 10000;

String message="";
String stat = "RAA:active\nLoc:";

#define relay 8
#define led 7

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

float latitude = 0;
float longitude = 0; 
float speed_kph = 0;
float heading = 0;
float altitude = 0;

char fonaNotificationBuffer[64];          //for notifications from the FONA
char smsBuffer[64];
char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
char callerIDbuffer[32];                  //we'll store the SMS sender number in here

static const char url[] PROGMEM = "https://aepb-web-api.azurewebsites.net/api/v1/trucks/%s/locations";    // replace %s with device ID
static const char glo_apn[] PROGMEM = "APN";    // replace %s with device ID
static const char glo_password[] PROGMEM = "Flat";
static const char glo_username[] PROGMEM = "Flat";    

char data_c[50] = {};
String data = "{\"longitude\":<lat>,\"lattitude\":<lon>}";

static const char ID[16] = "ABCDEFGHIKLM";
uint16_t smslen;
int slot;
int charCount;
int status_code;
int length;
unsigned int upload_timeout = 300;
unsigned long last_upload_time = 0;

boolean myLocation() {
  boolean gps_success = false;
  unsigned long previous = millis();
  while(millis() - previous<fixtime) {
    gps_success = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
    Serial.print(gps_success);
    if(gps_success) return true;
  }
  return gps_success;
}

void send_to_prunedge_server(void) {

  data = data.replace((char *)"<lat>", (char *)String(latitude, 6).c_str());
  data = data.replace((char *)"<lon>", (char *)String(longitude, 6).c_str());

  data.toCharArray(data_c, (unsigned int)strlen(data_c));

  if (!fona.HTTP_POST_start(url, F("application/json"), (uint8_t *)data_c, strlen(data_c), &status_code, (uint16_t *)&length))
  {
    Serial.println("Failed to make HTTP post");
  }
  else {
    Serial.print(status_code); Serial.println(" OK");
  }

  fona.HTTP_POST_end();
}


void setup() {

  pinMode(relay, OUTPUT);
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  while (! Serial);
  Serial.begin(9600);

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

  fona.setGPRSNetworkSettings(glo_apn, glo_username, glo_password);
  fona.enableGPRS(true);
  fona.setHTTPSRedirect(true);

  fona.getIMEI(ID);

  //fonaSerial->print("AT+CNMI=2,1\r\n");  //set up the FONA to send a +CMTI notification when an SMS is received
  Serial.println("FONA Ready");

}

  
void loop() {

  char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer

  if (fona.available())      //any data available from the FONA?
  {
    slot = 0;            //this will be the slot number of the SMS
    charCount = 0;
  
    do {      //Read the notification into fonaInBuffer
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer) - 1)));

    *bufPtr = 0;

    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);

      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) Serial.println("SMS not in slot!");
  
      Serial.print(F("SMS from: ")); Serial.println(callerIDbuffer);
  
      if(fona.readSMS(slot, smsBuffer, 64, &smslen)) {
        
        Serial.println(smsBuffer);
        
        /* Remote commands are executed here
         * See definitions above.
        */
        if( strstr(smsBuffer, mystatus) == 0 ) {
          if(myLocation()) {
            message = googlemap + String(latitude,6)+","+String(longitude,6);
            message = stat + message +"\nSpeed:"+ (String)speed_kph + "KPH";
            if(!fona.sendSMS(callerIDbuffer, message.c_str())) Serial.println(F("Failed to send respons"));
          }
          else {
            Serial.println(F("Response success"));
          }
        } 
        else if (strstr(smsBuffer, kill) == 0) {   // relay handler
          stat = "RAA: killed\nLoc:";
          digitalWrite(relay, HIGH);
          Serial.println("vTrack stoped!");
          if (!fona.sendSMS(callerIDbuffer, "RAA:killed")) {
            Serial.println(F("Realay resp: Failed"));
          }
        }
        else if (strstr(smsBuffer, allow) == 0) {
          digitalWrite(relay, LOW);
          //control = false;
          stat = "RAA: active\nLoc:";
          if (!fona.sendSMS(callerIDbuffer, "RAA:Active")) {
            Serial.print(F("Failed"));
          }
        }
      }
    }
  }

  if (millis() - last_upload_time > upload_timeout) {
    last_upload_time = millis();
    myLocation();
    send_to_prunedge_server();
  }
}
