// https://github.com/cotestatnt/AsyncTelegram2
#include <AsyncTelegram2.h>

// Timezone definition
#include <time.h>
#define MYTZ "WIB-7"
char formattedTime[64];

#include <WiFi.h>
#include <WiFiClient.h>
#if USE_CLIENTSSL
#include "tg_certificate.h"
#include <SSLClient.h>
WiFiClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, A0, 1, SSLClient::SSL_ERROR);
#else
#include <WiFiClientSecure.h>
WiFiClientSecure client;
#endif

AsyncTelegram2 myBot(client);

const char* ssid = "Legiun";
const char* pass = "sembada33";

const char* token = "6902776190:AAGs-kSL-s7-JloUkLkGKpPZtFUqMj-G2tY";
int64_t userid = 1226142648;

const char TEXT_HELP[] = "Available Commands:\n"
                         "/pakan - Aktifasi servo\n"
                         "/cek_suhu  - Cek suhu aquarium\n"
                         "/cek_ph - Cek nilai pH pada aquarium";

float setVph7;
float setVph4;
float setOffsetPH;

// function
void inputOutputSetup();
void feeding();
float dallas(int x, byte start);
void updateDataPoints(float newValue);
void calculateSMA();
float calculateEMA(float previousEMA, float alpha);
float calculatePH(float voltage);
float readingPHsensor();
bool longPress(int btnPin);

bool button();

// inout
#include "eeReadWrite.h"
#include "inputOutput.h"

void setup()
{
    Serial.begin(115200);

    EEPROM.begin(128);

    inputOutputSetup();

    WiFi.setAutoConnect(true);
    WiFi.mode(WIFI_STA);

    // connects to the access point and check the connection
    WiFi.begin(ssid, pass);
    delay(250);

    Serial.println("Connecting to Network");

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        digitalWrite(ledPin, !digitalRead(ledPin));
        delay(50);
    }

    Serial.print("\nWiFi connect to: "), Serial.println(ssid);

    delay(1000);

    // Sync time with NTP
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#if USE_CLIENTSSL == false
    client.setCACert(telegram_cert);
#endif

    // Set the Telegram bot properies
    myBot.setUpdateTime(1000);
    myBot.setTelegramToken(token);

    // Check if all things are ok
    Serial.print("\nTest Telegram connection... ");
    myBot.begin() ? Serial.println("OK") : Serial.println("NOK");

    char welcome_msg[128];
    snprintf(welcome_msg, 128, "BOT @%s online\n/start all commands avalaible.", myBot.getBotName());
    myBot.sendTo(userid, welcome_msg);

    // setVph4 = 3.20;
    // setVph7 = 2.50;

    memReadFloat(&setVph7, 0);
    memReadFloat(&setVph4, 4);
    memReadFloat(&setOffsetPH, 10);

    Serial.print("\nvph7: "), Serial.print(setVph7);
    Serial.print("\tvph4: "), Serial.print(setVph4);
    Serial.print("\tOffsetPH: "), Serial.println(setOffsetPH);
}

bool ph4Awaiting = false;
bool ph7Awaiting = false;
bool offsetAwaiting = false;
bool resetNow = false;

void loop()
{
    formatTime();

    // temperature
    currentTemp = dallas(dsPin, 0);

    // ph sensor
    readingPHsensor();

    // blink the led for 50ms and display the time
    static uint32_t displayTF = millis();
    if (millis() - displayTF < 50)
        digitalWrite(ledPin, HIGH);
    if (millis() - displayTF > 50)
        digitalWrite(ledPin, LOW);
    if (millis() - displayTF > 2000) {
        Serial.print(formattedTime);
        Serial.print("\tvpH: ");
        Serial.print(finalVoltage);
        Serial.print("\tpH: ");
        Serial.print(readingPHsensor(), 1);
        Serial.print("\tT: ");
        Serial.print(currentTemp);
        Serial.println();
        displayTF = millis();
    }

    if (longPress(buttonPin)) {
        digitalWrite(ledPin, HIGH);

        delay(2000);

        ESP.restart();
    }

    if (button()) {
        digitalWrite(ledPin, LOW);

        String reply;
        reply += "Test Komunikasi: ";
        reply += formattedTime;
        reply += "\nPakan diberikan";
        reply += "\nSuhu: ";
        reply += currentTemp;
        reply += "°C";
        reply += "\nNilai pH:";
        reply += readingPHsensor();

        Serial.println();
        Serial.print(reply);

        isFeeding = true;
        feeding();

        delay(1000);
        myBot.sendTo(userid, reply);
    }

    // Check incoming messages and keep Telegram server connection alive
    TBMessage msg;

    // check new messages
    if (myBot.getNewMessage(msg)) {
        Serial.print(msg.sender.username);
        Serial.print(" send this: ");
        Serial.println(msg.text);

        String msgText = msg.text;

        if (msgText.equals("/start"))
            myBot.sendMessage(msg, TEXT_HELP);

        else if (msgText.equals("/pakan")) {
            digitalWrite(ledPin, HIGH);

            isFeeding = true;

            String reply;
            reply += "Pakan Ikan Di Berikan";
            reply += "\ndiberikan pukul: ";
            reply += formattedTime;
            myBot.sendMessage(msg, reply);

            feeding();

            delay(500);
        }

        else if (msgText.equals("/cek_suhu")) {
            digitalWrite(ledPin, HIGH);

            String reply;
            reply += "Suhu saat ini: ";
            reply += currentTemp;
            reply += "°C";
            reply += "\ndiambil pukul: ";
            reply += formattedTime;
            myBot.sendMessage(msg, reply);

            delay(500);
        }

        else if (msgText.equals("/cek_ph")) {
            digitalWrite(ledPin, HIGH);

            String reply;
            reply += "Nilai pH saat ini: ";
            reply += readingPHsensor();
            reply += "\ndiambil pukul: ";
            reply += formattedTime;
            myBot.sendMessage(msg, reply);

            delay(500);
        }

        // kalibrasiManual();

        // set the voltage of sensor module reading, than used it in calculatePH function
        // voltage ph7
        else if (msgText.equals("/set_ph7")) {
            ph7Awaiting = true; // waiting for the voltage value
            myBot.sendMessage(msg, "Please send the desired voltage value for pH7 sensor.");
            delay(500);
        }

        else if (ph7Awaiting) {
            // Extract the float value
            float vph7 = msgText.toFloat();
            setVph7 = vph7;

            // Send confirmation message back to the user
            String reply = "Voltage of ph7 is: ";
            reply += String(vph7) + "V";
            myBot.sendMessage(msg, reply);

            ph7Awaiting = false; // Reset the flag since we have received the voltage value

            delay(500);

            memWriteFloat(&vph7, 0);
            EEPROM.commit();
        }

        // voltage ph4
        else if (msgText.equals("/set_ph4")) {
            ph4Awaiting = true; // waiting for the voltage value
            myBot.sendMessage(msg, "Please send the desired voltage value for pH4 sensor.");
            delay(500);
        }

        else if (ph4Awaiting) {
            // Extract the float value
            float vph4 = msgText.toFloat();
            setVph4 = vph4;

            // Send confirmation message back to the user
            String reply = "Voltage of ph4 is: ";
            reply += String(vph4) + "V";
            myBot.sendMessage(msg, reply);

            ph4Awaiting = false; // Reset the flag since we have received the voltage value

            delay(500);

            memWriteFloat(&vph4, 4);
            EEPROM.commit();
        }

        // voltage ph4
        else if (msgText.equals("/set_offset")) {
            offsetAwaiting = true; // waiting for the voltage value
            myBot.sendMessage(msg, "Please send desired value for offset of pH module.");
            delay(500);
        }

        else if (offsetAwaiting) {
            // Extract the float value
            float offset = msgText.toFloat();
            setOffsetPH = offset;

            // Send confirmation message back to the user
            String reply = "offset is: ";
            reply += "\npH Final = pH Final - ";
            reply += String(offset);
            myBot.sendMessage(msg, reply);

            offsetAwaiting = false; // Reset the flag since we have received the voltage value

            delay(500);

            memWriteFloat(&offset, 10);
            EEPROM.commit();
        }
    }
}

// get the formatted time and store to formattedTime variable
void formatTime()
{
    time_t now = time(nullptr);
    struct tm t = *localtime(&now);
    // char formattedTime[64];
    strftime(formattedTime, sizeof(formattedTime), "%X", &t);
    // return String(msgBuffer);
}
