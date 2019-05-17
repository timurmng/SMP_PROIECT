#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include "RTClib.h"

#define RX 3
#define TX 2
#define SLP_HPA (1013.25) // Sea Level Pressure (hPa)
#define ALTITUDE_HOME (131)

RTC_DS1307 RTC;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
SoftwareSerial esp8266(RX, TX);
Adafruit_BMP280 bmp;

// ESP8266 WI-FI MODULE & THINGSPEAK DATA
String AP = "Nu servim";
String PASS = "Gestapo69.";
String API = "2M5GQIT0VBNJWU37";
String HOST = "api.thingspeak.com";
String PORT = "80";
String tempTS = "field1";
String pressTS = "field2";

int countTrueCommand;
int countTimeCommand;
boolean found = false;

byte counter = 0;
byte delta_time = 0;
int t_hour = 0;
int t_minute = 0;
float pressureArray[10] = {0};

void setup() {

    Serial.begin(9600);
    Wire.begin();
    bmp.begin();
    lcd.begin(16, 2);
    esp8266.begin(115200);
    RTC.begin();
    RTC.adjust(DateTime(__DATE__, __TIME__));

    sendCommand("AT", 5, "OK");
    sendCommand("AT+CWMODE=1", 5, "OK");
    sendCommand("AT+CWJAP=\"" + AP + "\",\"" + PASS + "\"", 20, "OK");

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}

void loop() {
    lcd.clear();
    float pressure = bmp.readPressure() / 100.0F;
    float temperature = bmp.readTemperature();
    float seapressure = station2sealevel(pressure, ALTITUDE_HOME, temperature);

    printPressureLCD(seapressure);
    printTemperatureLCD(temperature);

    DateTime now = RTC.now();
    printSerialDate(now);
    int t_hour2 = now.hour();
    int t_minute2 = now.minute();
    int Z = 0;

    if (t_hour2 != t_hour or t_minute2 != t_minute) {
        delta_time++;
        if (delta_time > 10) {
            delta_time = 0;
            if (counter == 10) {
                for (int i = 0; i < 9; i++) {
                    pressureArray[i] = pressureArray[i + 1];
                }
                pressureArray[counter - 1] = seapressure;
            } else {
                pressureArray[counter] = seapressure;
                counter++;
            }
        }
    }

    Z = calcZambretti((pressureArray[9] + pressureArray[8] + pressureArray[7]) / 3,
                      (pressureArray[0] + pressureArray[1] + pressureArray[2]) / 3, now.month());
    lcd.setCursor(7, 0);
    lcd.print(interpretWeather(Z, seapressure));
    lcd.setCursor(12, 1);
    lcd.print("Z=");
    lcd.print(Z);
      
    t_hour=t_hour2;
    t_minute=t_minute2;
}

void printSerialDate(DateTime now) {
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
}

void sendData(float auxTemp, float auxPres) {
    Serial.print("Pressure: " + String(auxPres, 2) + "        Temp: " + String(auxTemp, 1));
    Serial.println();
    String getData = "GET /update?api_key=" + API + "&" + tempTS + "=" + String(auxTemp, 2) + "&" + pressTS + "=" +
                     String(auxPres, 2);
    sendCommand("AT+CIPMUX=1", 1, "OK");
    sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 1, "OK");
    sendCommand("AT+CIPSEND=0," + String(getData.length() + 4), 1, ">");
    esp8266.println(getData);
    delay(250);
    countTrueCommand++;
    sendCommand("AT+CIPCLOSE=0", 1  , "OK");
}

void sendCommand(String command, int maxTime, char readReplay[]) {
    Serial.print(countTrueCommand);
    Serial.print(". at command => ");
    Serial.print(command);
    Serial.print(" ");
    while (countTimeCommand < (maxTime * 1)) {
        esp8266.println(command);//at+cipsend
        if (esp8266.find(readReplay)) //ok
        {
            found = true;
            break;
        }

        countTimeCommand++;
    }

    if (found == true) {
        Serial.println("OYI");
        countTrueCommand++;
        countTimeCommand = 0;
    }

    if (found == false) {
        Serial.println("Fail");
        countTrueCommand = 0;
        countTimeCommand = 0;
    }

    found = false;
}

void printPressureLCD(float pressure) {
    String presString = String(pressure, 0);
    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(presString);
    lcd.print("hPa");
}

void printTemperatureLCD(float temperature) {
    String tempString = String(temperature, 0);
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(tempString);
    lcd.print((char) 223);
    lcd.print("C");
}


float station2sealevel(float p, int height, float t) {
    return (double) p * pow(1 - 0.0065 * (double) height / (t + 0.0065 * (double) height + 273.15), -5.275);
}

void printSerial(String s) {
    Serial.print(s);
    Serial.println();
}

int calcZambretti(float currPressure, float prevPressure, int mon) {
    if (currPressure < prevPressure) {
        //FALLING
        if (mon >= 4 and mon <= 9) {
            // SUMMER
            if (currPressure >= 1030)
                return 2;
            else if (currPressure >= 1020 and currPressure < 1030)
                return 8;
            else if (currPressure >= 1010 and currPressure < 1020)
                return 18;
            else if (currPressure >= 1000 and currPressure < 1010)
                return 21;
            else if (currPressure >= 990 and currPressure < 1000)
                return 24;
            else if (currPressure >= 980 and currPressure < 990)
                return 24;
            else if (currPressure >= 970 and currPressure < 980)
                return 26;
            else if (currPressure < 970)
                return 26;
        } else {
            //winter
            if (currPressure >= 1030)
                return 2;
            else if (currPressure >= 1020 and currPressure < 1030)
                return 8;
            else if (currPressure >= 1010 and currPressure < 1020)
                return 15;
            else if (currPressure >= 1000 and currPressure < 1010)
                return 21;
            else if (currPressure >= 990 and currPressure < 1000)
                return 22;
            else if (currPressure >= 980 and currPressure < 990)
                return 24;
            else if (currPressure >= 970 and currPressure < 980)
                return 26;
            else if (currPressure < 970)
                return 26;
        }
    } else if (currPressure > prevPressure) {
        //RAISING
        if (mon >= 4 and mon <= 9) {
            //summer
            if (currPressure >= 1030)
                return 1;
            else if (currPressure >= 1020 and currPressure < 1030)
                return 2;
            else if (currPressure >= 1010 and currPressure < 1020)
                return 3;
            else if (currPressure >= 1000 and currPressure < 1010)
                return 7;
            else if (currPressure >= 990 and currPressure < 1000)
                return 9;
            else if (currPressure >= 980 and currPressure < 990)
                return 12;
            else if (currPressure >= 970 and currPressure < 980)
                return 17;
            else if (currPressure < 970)
                return 17;
        } else {
            //winter
            if (currPressure >= 1030)
                return 1;
            else if (currPressure >= 1020 and currPressure < 1030)
                return 2;
            else if (currPressure >= 1010 and currPressure < 1020)
                return 6;
            else if (currPressure >= 1000 and currPressure < 1010)
                return 7;
            else if (currPressure >= 990 and currPressure < 1000)
                return 10;
            else if (currPressure >= 980 and currPressure < 990)
                return 13;
            else if (currPressure >= 970 and currPressure < 980)
                return 17;
            else if (currPressure < 970)
                return 17;
        }
    } else {
        if (currPressure >= 1030)
            return 1;
        else if (currPressure >= 1020 and currPressure < 1030)
            return 2;
        else if (currPressure >= 1010 and currPressure < 1020)
            return 11;
        else if (currPressure >= 1000 and currPressure < 1010)
            return 14;
        else if (currPressure >= 990 and currPressure < 1000)
            return 19;
        else if (currPressure >= 980 and currPressure < 990)
            return 23;
        else if (currPressure >= 970 and currPressure < 980)
            return 24;
        else if (currPressure < 970)
            return 26;

    }
}

String interpretWeather(int Z, float seapressure) {
    if (pressureArray[9] > 0 and pressureArray[0] > 0) {
        if (pressureArray[9] + pressureArray[8] + pressureArray[7] - pressureArray[0] - pressureArray[1] -
            pressureArray[2] >= 3) {
            printSerial("RAISING");
            if (Z < 3)
                return "Sunny";
            else if (Z >= 3 and Z <= 9)
                return "Overcast";
            else if (Z > 9 and Z <= 17)
                return "Cloudy";
            else if (Z > 17)
                return "Rainy";

        } else if (pressureArray[0] + pressureArray[1] + pressureArray[2] - pressureArray[9] - pressureArray[8] -
                   pressureArray[7] >= 3) {
            printSerial("FALLING");
            if (Z < 4)
                return "Sunny";
            else if (Z >= 4 and Z < 14)
                return "Overcast";
            else if (Z >= 14 and Z < 19)
                return "Worsening";
            else if (Z >= 19 and Z < 21)
                return "Cloudy";
            else if (Z >= 21)
                return "Rainy";

        } else {
            printSerial("STEADY");
            if (Z < 5)
                return "Sunny";
            else if (Z >= 5 and Z <= 11)
                return "Overcast";
            else if (Z > 11 and Z < 14)
                return "Cloudy";
            else if (Z >= 14 and Z < 19)
                return "Worsening";
            else if (Z > 19) {
                return "Rainy";
            }
        }
    } else {
        printSerial("Inca se calculeaza");
        if (seapressure < 1005)
            return "Rainy";
        else if (seapressure >= 1005 and seapressure <= 1015)
            return "Cloudy";
        else if (seapressure > 1015 and seapressure < 1025)
            return "Overcast";
        else
            return "Rainy";
    }
}
