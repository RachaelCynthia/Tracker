#include "Adafruit_FONA.h"
#include <Adafruit_FONA.h>
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

#define gloAPN "gloflat"
#define gloUSERNAME "flat"
#define gloPASSWORD "flat"

uint16_t fixtime = 1000;

String message = "";
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

char fonaNotificationBuffer[64]; //for notifications from the FONA
char smsBuffer[250];
char *bufPtr = fonaNotificationBuffer; //handy buffer pointer

String url = "http://aepb-web-api.azurewebsites.net/api/v1/trucks/<url>/locations"; // replace %s with device ID
// const char glo_apn[] = "APN";                                                           // replace %s with device ID
// char glo_password[] = "Flat";
// char glo_username[] = "Flat";

char data_c[50] = {};
String data = "{\"longitude\":<lat>,\"lattitude\":<lon>}";

int status_code = 0;
int length = 0;
unsigned long upload_timeout = 300000;
unsigned long last_upload_time = 0;
uint16_t smslen;
int slot;
int charCount;

boolean myLocation()
{
    boolean gps_success = false;
    unsigned long previous = millis();
    while (millis() - previous < fixtime)
    {
        gps_success = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
        if (gps_success)
            return true;
    }
    return gps_success;
}

void send_to_prunedge_server(void)
{
    fona.enableGPRS(true);
    data.replace("<lat>", String(latitude, 6));
    data.replace("<lon>", String(longitude, 6));
    data.toCharArray(data_c, (unsigned int)strlen(data_c));
    if (!fona.HTTP_POST_start(url.c_str(), F("application/json"), (uint8_t *)data_c, strlen(data_c), &status_code, (uint16_t *)&length))
    {
        Serial.println(F("Failed to make HTTP post"));
    }
    Serial.print(status_code);
    Serial.println(F(" status code"));
    Serial.print(length);
    Serial.println(F(" length"));
    data = "{\"longitude\":<lat>,\"lattitude\":<lon>}";
    status_code = 0;
    length = 0;
    fona.HTTP_POST_end();
    fona.enableGPRS(false);
}

void setup()
{

    pinMode(relay, OUTPUT);
    pinMode(led, OUTPUT);
    digitalWrite(led, LOW);

    while (!Serial)
        ;
    Serial.begin(9600);

    fonaSerial->begin(9600);
    if (!fona.begin(*fonaSerial))
    {
        while (1)
            ;
    }

    Serial.println(F("FONA is OK"));
    // Try to enable GPRS
    Serial.println(F("Enabling GPS..."));
    fona.enableGPS(true);
    fonaSerial->print("AT+CNMI=2,1\r\n");
    digitalWrite(led, HIGH);

    /*
   * <username> and <password> is the same for Glo network
   * fona.setGPRSNetworkSettings(glo_apn, <username>, <password?);
  */
    // fona.setGPRSNetworkSettings(F(gloAPN), F(gloUSERNAME), F(gloPASSWORD));
    // fona.enableGPRS(true);
    // fona.setHTTPSRedirect(true);

    char ID[16] = {0};
    if (fona.getIMEI(ID) > (uint8_t)0)
    {
        url.replace("<url>", String(ID));
        Serial.print("Complete URL: ");
        Serial.println(url);
    }
    else
    {
        Serial.println("Did not get SIM IMEI");
    }
    Serial.println("FONA Ready");
}

void loop()
{

    char *bufPtr = fonaNotificationBuffer;      //handy buffer pointer

    if (fona.available())
    {
        slot = 0;
        charCount = 0;

        do
        {
            *bufPtr = fona.read();
            Serial.write(*bufPtr);
            delay(1);
        } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer) - 1)));

        *bufPtr = 0;

        if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot))
        {
            Serial.print("slot: ");
            Serial.println(slot);

            char callerIDbuffer[32]; //we'll store the SMS sender number in here
            if (!fona.getSMSSender(slot, callerIDbuffer, 31))
            {
                Serial.println("SMS not in slot!");
            }
            else
            {
                Serial.print(F("SMS from: "));
                Serial.println(callerIDbuffer);
            }

            if (fona.readSMS(slot, smsBuffer, 250, &smslen))
            {
                Serial.print("smsBuffer: ");
                Serial.println(smsBuffer);

                if (strstr(smsBuffer, kill) != NULL) // relay handler
                {
                    stat = "RAA: killed\nLoc:";
                    digitalWrite(relay, HIGH);
                    Serial.println(F("vTrack stoped!"));
                    if (!fona.sendSMS(callerIDbuffer, "RAA:killed"))
                    {
                        Serial.println(F("Kill response: Failed"));
                    }
                }
                else if (strstr(smsBuffer, allow) != NULL)
                {
                    digitalWrite(relay, LOW);
                    //control = false;
                    stat = "RAA: active\nLoc:";
                    if (!fona.sendSMS(callerIDbuffer, "RAA:Active"))
                    {
                        Serial.print(F("Failed"));
                    }
                    else
                    {
                        Serial.println("Allow response: Failed");
                    }
                }
                else if (strstr(smsBuffer, mystatus) != NULL)
                {
                    if (myLocation())
                    {
                        message = googlemap + String(latitude, 6) + "," + String(longitude, 6);
                        message = stat + message + "\nSpeed:" + (String)speed_kph + "KPH";
                        if (!fona.sendSMS(callerIDbuffer, message.c_str()))
                        {
                            Serial.println(F("Failed to send mystatus response"));
                        }
                        else
                        {
                            Serial.println(F("Response success"));
                        }
                    }
                    else
                    {
                        Serial.println("Location unavailable");
                    }
                }
                else
                {
                    if (!fona.sendSMS(callerIDbuffer, "Command not recognised"))
                        Serial.println("Replying to unknown command: Failed");
                }
            }

            Serial.print(F("smsBuffer: ")); Serial.println(smsBuffer);
            for (int i = 0; i < sizeof(smsBuffer)/sizeof(smsBuffer[0]); i++)
            {
                smsBuffer[i] = 0;
            }

            if (!fona.deleteSMS(slot))
            {
                Serial.println(F("Failed to deleted SMS slot"));
                fona.print(F("AT+CMGD=?\r\n"));
            }
            else
            {
                Serial.print(F("slot "));
                Serial.print(slot);
                Serial.println(F(" deleted"));
            }
        }
    }

    if (millis() - last_upload_time > upload_timeout)
    {
        last_upload_time = millis();
        Serial.println(F("Supposed to send to interent here."));
        //myLocation();
        //send_to_prunedge_server();
    }
}
