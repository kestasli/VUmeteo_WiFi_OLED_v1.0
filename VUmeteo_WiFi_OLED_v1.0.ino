

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <ESP8266_SSD1322.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold44pt7b.h>

//ESP8266 Pins
#define OLED_CS     15  // Pin 19, CS - Chip select
#define OLED_DC     2   // Pin 20 - DC digital signal
#define OLED_RESET  16  // Pin 15 -RESET digital signal

const char host[] = "eismoinfo.lt";
const int port = 80;
const char url[] = "GET /weather-conditions-retrospective?id=310&number=1 HTTP/1.1";

//const char host[] = "www.hkk.gf.vu.lt";
//const int port = 80;
//const char url[] = "GET /json.php HTTP/1.1";

boolean configmode = false;
boolean debugmode = false;

unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
//long interval = 5000;
long interval = 60000;

int configPin = 5;
int configPinExt = 4;

//matavimu kiekis atmosferos slegio irasams
const int numOfMeasures = 24;
//i masyva itraukia kas N slegio matavima, nes slegis letai kinta
const int skipMeasures = 180;
//slegio matavimu masyvas
float pressureArray[numOfMeasures];
//data reading sequence
int readingSeq = 0;

//klaidu kiekis del neteisingo nuskaitymo arba parsinimo
int errCount = 0;

float temperature;
float windspeed;
float dewpoint;
int humidity;
float pressure;


ESP8266_SSD1322 display(OLED_DC, OLED_RESET, OLED_CS);

void setup() {

  Serial.begin(115200);
  delay(10);
  display.begin(true);
  pinMode(configPin, INPUT_PULLUP);
  pinMode(configPinExt, INPUT_PULLUP);

  //ESP.eraseConfig();

  if (digitalRead(configPin) == LOW) {
    configmode = true;
  }

  //CONFIG mode start

  if (configmode) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    wifiManager.startConfigPortal("iTermometras");
  }

}

void loop() {

  currentMillis = millis();

  if (currentMillis - previousMillis > interval | previousMillis == 0) {

    previousMillis = currentMillis;

    //int datastatus = readDataVU();
    int datastatus = readDataKD();

    if (datastatus == 0) {

      display.clearDisplay();
      showTemp(temperature);
      //drawGraph(pressureArray, numOfMeasures, pressure);
      drawBars(windspeed);
      display.display();

    } else {
      //skaiciuojame klaidu kieki ir po kazkokio kiekio parodome klaida displejuje
      errCount++;
      Serial.print("Error count: ");
      Serial.println(errCount);
    }
  }

  delay (10);

}

//-------------------------------------------------------------
//--------------funkcijos--------------------------------------
//-------------------------------------------------------------

//read JSON data from eismoinfo.lt server

int readDataKD() {
  char jsonData[512];

  //set JSON buffer
  StaticJsonBuffer<512> jsonBuffer;
  WiFiClient client;
  if (client.connect(host, port)) {

    //client.setTimeout(2000);
    Serial.println("Connected to server");
    client.println(url);
    client.print("Host: ");
    client.println(host);
    client.println("Connection: close");
    client.println("\r\n\r\n");

    delay(100);

    if (client.find("[")) {

      Serial.println("Header found");

      int i = 0;

      while (client.available()) {
        jsonData[i] = client.read();;
        i++;
      }
      jsonData[i] = '\0';

      //Serial.println(jsonData);

      JsonObject& root = jsonBuffer.parseObject(jsonData);

      if (!root.success()) {
        Serial.println("parseObject() failed");
        return 1;
      }

      temperature = root["oro_temperatura"];
      windspeed = root["vejo_greitis_vidut"];
      dewpoint = root["rasos_taskas"];
      humidity = getHumidity(temperature, dewpoint);

      //Serial.print("Reading Nr: ");
      //Serial.println(readingSeq);
      Serial.print("Temperature: ");
      Serial.println(temperature);
      Serial.print("Windspeed: ");
      Serial.println(windspeed);
      Serial.print("Dewpoint: ");
      Serial.println(dewpoint);
      Serial.print("Humidity: ");
      Serial.println(humidity);


    }

  } else {
    Serial.println ("Unable to connect to server");
    client.stop();
    return 1;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("Disconnecting from server.");
    client.stop();
  }
  return 0;
}

//read JSON data from VU server

int readDataVU() {
  char jsonData[512];

  //set JSON buffer
  StaticJsonBuffer<512> jsonBuffer;
  WiFiClient client;

  if (client.connect(host, port)) {

    //client.setTimeout(2000);
    Serial.println("Connected to server");
    client.println(url);
    client.print("Host: ");
    client.println(host);
    client.println("Connection: close");
    client.println("\r\n\r\n");

    delay(500);

    if (client.find("wup(")) {

      Serial.println("Header found");

      int i = 0;

      while (client.available()) {
        jsonData[i] = client.read();;
        i++;
      }
      jsonData[i] = '\0';

      //Serial.println(jsonData);

      JsonObject& root = jsonBuffer.parseObject(jsonData);

      if (!root.success()) {
        Serial.println("parseObject() failed");
        return 1;
      }

      temperature = root["zeno_AT_5s_C"];
      windspeed = root["zeno_gust"];
      humidity = root["zeno_RH_5s"];
      pressure = root["zeno_BP1_5s_Mb"];

      Serial.print("Temperature: ");
      Serial.println(temperature);
      Serial.print("Windspeed: ");
      Serial.println(windspeed);
      Serial.print("Humidity: ");
      Serial.println(humidity);
      Serial.print("Pressure: ");
      Serial.println(pressure);

    }

  } else {
    Serial.println ("Unable to connect to server");
    client.stop();
    return 1;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("Disconnecting from server.");
    client.stop();
  }
  return 0;
}



// windspeed vizualizacija su bars'ais
void drawBars(float windspeed) {

  int barWidth = 4;
  int barSpace = 1;
  int barMaxHeight = 16;
  int valueCount = 10;

  //grafiko pradzios koordinates
  int x = 204;
  int y = 38;
  //bar'o x ir y koordinates
  int barY = 0;
  int barX = 0;
  //int valueCount = sizeof(valueArray)/sizeof(float);

  display.fillRect(x, y + barMaxHeight + 1, valueCount * (barWidth + barSpace) - barSpace, 9, WHITE);

  display.setFont();
  display.setTextColor(BLACK);
  //pataisyti, kad Y koordinate butu isskaiciuojama automatiskai
  display.setCursor(x + 1, y + barMaxHeight + 2);
  display.print(windspeed, 1);
  display.print("m/s");

  int barNumber = (int)(windspeed + 0.5);

  if (barNumber >= valueCount) {
    barNumber = valueCount;
  }

  for (int i = 0 ; i < barNumber ; i++) {

    barY = y;
    barX = x + i * (barWidth + barSpace);
    display.fillRect( barX, barY, barWidth, barMaxHeight , WHITE);

  }

}

//
//nupiesia bar graph pagal uzpildyto masyvo reiksmes, HPA parodo apacioje
//
void drawGraph(float valueArray[], int valueCount, float hpa) {

  int barWidth = 2;
  int barSpace = 0;
  int barMaxHeight = 22;

  //grafiko pradzios koordinates
  int x = 204;
  int y = 0;
  //bar'o x ir y koordinates
  int barY = 0;
  int barX = 0;
  int barHeight = 0;

  display.fillRect(x, y + barMaxHeight + 1, valueCount * (barWidth + barSpace) - barSpace, 9, WHITE);

  display.setFont();
  display.setTextColor(BLACK);
  display.setCursor(x + 1 , y + barMaxHeight + 2);
  display.print(hpa, 0);
  display.print("hPa");

  //randame min ir max vertes is masyvo
  float barMin = getMin(valueArray, valueCount);
  float barMax = getMax(valueArray, valueCount);
  float average = getAverage(valueArray, valueCount);

  /*
    Serial.print("barMin: ");
    Serial.println(barMin);

    Serial.print("barMax: ");
    Serial.println(barMax);

    Serial.print("AVG: ");
    Serial.println(average);

    Serial.println();
  */

  float barHeightUnit = barMaxHeight / (barMax - barMin);

  for (int i = 0 ; i < valueCount ; i++) {

    barHeight = (int)((valueArray[i] - barMin) * barHeightUnit);
    //Serial.println(barHeight);

    barY = y + barMaxHeight - barHeight;
    barX = x + i * (barWidth + barSpace);

    if (valueArray[i] != 0) {

      //funkcija paiso uzpildytus BARS'us
      //display.fillRect( barX, barY, barWidth, barHeight , WHITE);

      //funkcija paiso 2pt aukscio linija
      display.fillRect( barX, barY, barWidth, 2 , WHITE);

    }
  }

}

//
//suranda maksimalia masyvo reiksme, bet neskaiciuoja nuliu
//

float getMax(float value[], int valueCount) {

  float maxValue;

  maxValue = getAverage(value, valueCount) + 3;

  //Tikriname, ar masyve yra didesniu verciu negu nustatyta
  for (int i = 0 ; i < valueCount; i++) {
    //patikrinam, ar yra verte mazesne uz minimalia.
    //Taip pat jeigu masyvas uzpildytas 0, to neskaiciuojame kaip min vertes.
    if (maxValue < value[i] & (int)value[i] != 0) {
      maxValue = value[i];
    }
  }
  return maxValue;

}

//suranda minimalia masyvo reiksme, bet neskaiciuoja nuliu
float getMin(float value[], int valueCount) {

  float minValue;

  minValue = getAverage(value, valueCount) - 3;

  for (int i = 0 ; i < valueCount; i++) {
    //patikrinam, ar yra verte mazesne uz minimalia.
    //Taip pat jeigu masyvas uzpildytas 0, to neskaiciuojame kaip min vertes.
    if (minValue > value[i] & (int)value[i] != 0) {
      minValue = value[i];
    }
  }
  return minValue;

}

float getAverage(float value[], int valueCount) {

  float arraySum = 0;
  int nonZeroCount = 0;

  //surandame vidurki

  for (int i = 0; i < valueCount; i++) {
    arraySum = arraySum + value[i];
    if ((int)value[i] != 0) {
      nonZeroCount++;
    }
  }

  float average = arraySum / nonZeroCount;
  return average;
}

//
//parodo didelius temperaturos skaicius
//

void showTemp(float temp) {
  //Temperaturos eilutei skiriame buferi pagal realu ilgi
  char tempstring[5];

  //konvertuojame double i char eilte
  dtostrf(temp, 5, 1, tempstring);

  display.setTextColor(WHITE);
  display.setFont(&FreeSansBold44pt7b);
  display.setTextWrap(false);
  display.setCursor(0, 61);
  display.print(tempstring);

}


void showHumidity(float humidity){
  
}

//
//pakeicia nuskaitytame tekste kableli i taska ir konvertuoja i float tipa
//

float convertChar(char *a) {

  float result;
  int i = 0;

  while (a[i] != '\0' | i <= 6) {

    if (a[i] == ',') {
      a[i] = '.';
    }
    i++;
  }

  result = atof (a);
  return result;
}

//Apskaiciuoja santykini dregnuma is dewpoint ir temperaturos
float getHumidity(float T, float TD) {

  float humidity = 100 * pow((112 - 0.1 * T + TD) / (112 + 0.9 * T), 8);
  return humidity;

}
