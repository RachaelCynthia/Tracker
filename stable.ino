/*
*   Stable version with full functionality. This was derived from temp.ino and will
*   remain as the code for subsequent upgrade.
*
*   27th July 2021
*/

#include <Adafruit_FONA.h>
#include <SoftwareSerial.h>


#define USE_SERIAL 1

/*
* GPRS definitions
*/
#define APN "gloflat"   // "web.gprs.mtnnigeria.net"
#define USERNAME "flat" // "web"
#define PASSWORD "flat" // "web"

/*
* SMS definitions
*/
#define SMS_BUFFER_LEN 160  // max length of SMS receivable
#define SMS_SLOT_NUMBER 20 // change this according to the SIM card
#define EMERGENCY_TEL "08103708977"
#define ENABLE_SMS_NOTIFICATIONS "AT+CNMI=2,1\r\n"
#define DELETE_ALL_SMS "AT+CMGDA=\"DEL ALL\"\r\n" 
#define WAIT_FOR_DELETE 5000
#define KILL "kill"
#define ALLOW "allow"
#define STATUS "status"

/*
* standard pins for the FONA shield
*/
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

/*
* Interface pins
*/
#define RELAY 8
#define LED 7

/*
* Others
*/
#define GOOGLE_MAP "https://maps.google.com/maps?q="
#define URL "httpbin.org/anything"
#define WEB_DATA_MAX_SIZE 80    // actually 78 "lattitude" + "longitude" + length of numbers as strings + JSON format overhead + IMEI as string + others
#define FONA_NOTIFICATION_BUFFER_SIZE 64

#if (USE_SERIAL > 0)
    #define SHOW(X) Serial.print(F(X))
    #define SHOW_LINE(X) Serial.println(F(X))
#else
    #define SHOW(X) (void)0
    #define SHOW_LINE(X) (void)0
#endif

#define FLASH(X) F(X)


float latitude = 0;
float longitude = 0;
float speed_kph = 0;
float heading = 0;
float altitude = 0;

int slot = 0;
int charCount = 0;
int smsLen = 0;

const unsigned int server_upload_interval = 60000;  // in milliseconds
unsigned long last_upload_time = 0;

char fonaNotificationBuffer[FONA_NOTIFICATION_BUFFER_SIZE]; 
char receivedSMS[SMS_BUFFER_LEN + WEB_DATA_MAX_SIZE];
//char outgoingData[WEB_DATA_MAX_SIZE];
 
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

SoftwareSerial *fonaSerial = &fonaSS;
char *bufPtr = fonaNotificationBuffer;

boolean fetch_location(void);
void sms_handler(char * sms_buffer, char * sender_number);
void clear_buffer(char * buffer_);
void clear_sms_slots(void);
void send_to_server(char *data);


void setup() {

    pinMode(RELAY, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    digitalWrite(LED_BUILTIN, HIGH);    // indicates setup begins
    digitalWrite(RELAY, LOW);
    digitalWrite(LED, LOW);

    while (!Serial);
    while(!fona.begin(*fonaSerial));
    fona.enableGPS(true);
    fonaSerial->print(F(ENABLE_SMS_NOTIFICATIONS));

    fona.setGPRSNetworkSettings(F(APN), F(USERNAME), F(PASSWORD));
    fona.enableGPRS(true);
    fona.setHTTPSRedirect(true);
    
    digitalWrite(LED_BUILTIN, LOW);     // indicates setup completes
}

void loop() {

    if (millis() - last_upload_time >= server_upload_interval) {
        last_upload_time = millis();
        if (fetch_location()) {
            send_to_server(receivedSMS);    // SMS buffer unused at this point
        }
        else {
            SHOW_LINE("Location unavailable");
        }
    }

    if (fona.available()) {
        char *bufPtr = fonaNotificationBuffer;
        slot = 0;
        charCount = 0;

        do {
            *bufPtr = fona.read();
            Serial.write(*bufPtr);  // Remove this line for production
            delay(1);
        } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer) - 1)));
    
        *bufPtr = 0;
        
        if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
            char callerIDbuffer[32];
            if (!fona.getSMSSender(slot, callerIDbuffer, 31)) {
                SHOW_LINE("SMS not in slot");
            }
            else {
                if (fona.readSMS(slot, receivedSMS, SMS_BUFFER_LEN, &smsLen)) {
                    sms_handler(receivedSMS, callerIDbuffer);
                    clear_buffer(receivedSMS, SMS_BUFFER_LEN);
                    fona.print(F(DELETE_ALL_SMS));
                    delay(WAIT_FOR_DELETE);
                }
            }
        }       
    }
}

boolean fetch_location(void) {
    if (fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude)) {
        return true;
    }
    else {
        return false;
    }
}

void sms_handler(char * sms_buffer, char * sender_number) {
    /*
    * @param sms_buffer: an array holding the SMS command
    * @param sender_number: DUHHH !!!
    */
    if (strstr(sms_buffer, KILL) != NULL) {
        digitalWrite(RELAY, HIGH);
        fona.sendSMS(sender_number, "RAA:Killed");
        SHOW_LINE("Sent message for KILL");
    }
    else if (strstr(sms_buffer, ALLOW) != NULL) {
        digitalWrite(RELAY, LOW);
        fona.sendSMS(sender_number, "RAA:Active");
        SHOW_LINE("Sent message for KILL");
    }
    else if (strstr(sms_buffer, STATUS) != NULL) {
        if (fetch_location()) {
            sprintf(sms_buffer, "%s%s%s,%s\nSpeed:%sKPH", "RAA:active\nLoc:", (char *)GOOGLE_MAP,
                String(latitude, 6).c_str(),
                String(longitude, 6).c_str(),
                String(speed_kph, 2).c_str()
            );
            fona.sendSMS(sender_number, "RAA:Active");
            SHOW_LINE("Sent message for status");
        }
        else {
            SHOW_LINE("Location unavailable");
        }
    }
    else {
        fona.sendSMS(sender_number, "Invalid");
    }
}

void clear_buffer(char * buffer_, int buffer_l) {
    for (int i = 0; i < buffer_l; i++) buffer_[i] = 0;
}

void send_to_server(char * data) {
    uint16_t status_code = 0;
    uint16_t msgLen = 0;
    //const char * url = URL;
    fona.enableGPRS(true);
    char imei[16] = "ABCDEFGHIJKMN0";
    fona.getIMEI(imei);
    sprintf(data, "{\"longitude\":%s, \"lattitude\":%s, \"id\":%s}", String(longitude, 6).c_str(), String(latitude,6).c_str(), imei);
    fona.HTTP_POST_start(URL, F("application/json"), (uint8_t *)data, strlen(data), &status_code, (uint16_t *)&msgLen);
    SHOW("Status code: "); Serial.println(status_code);
    SHOW("Data length: "); Serial.println(msgLen);
    fona.HTTP_POST_end();
    fona.enableGPRS(false);
    // clear buffer?
}
