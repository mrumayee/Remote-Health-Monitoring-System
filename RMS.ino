/*-----------Mini Project 2021-2022---------

Name - Remote Health Monitoring System to Monitor Pulse rate and Temperature using NodeMCu and Ubidots Cloud.

*/
// I2C address of Oled 0x3C

#include <MAX30105.h>
#include <heartRate.h> // Heart rate calculating algo
#include<Wire.h>

#include <Adafruit_GFX.h>        //OLED libraries
#include <Adafruit_SSD1306.h>

// Oled Specs
#define Screen_Width 128 // OLED display width, in pixels
#define Screen_Height 64 // OLED display height, in pixels
#define Oled_Reset    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define LED_GREEN 0
#define LED_RED 2

#include "Ubidots.h"//library for Cloud Platform

//Wifi &Ubidots Credentials

const char* UBIDOTS_TOKEN = "BBFF-fkJBtKAikw93VsyG44wGQrJ4it7hhK";  // Put here your Ubidots TOKEN
const char* WIFI_SSID = "pk";      // Put here your Wi-Fi SSID
const char* WIFI_PASS = "pkpkpkpkpk";      // Put here your Wi-Fi password
Ubidots ubidots(UBIDOTS_TOKEN, UBI_HTTP);


MAX30105 particleSensor;
Adafruit_SSD1306 oled(Screen_Width, Screen_Height, &Wire, Oled_Reset);

//varibles declaration & Intialization

const byte RATE_SIZE = 20; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
int stable = 0;
int i = 0;
int no_of_loops = 10;
int alert_threshold = 5;
int last_value = 0;
int alert_count = 0;
int upperlimit = 90;
int lowerlimit = 75;

float temperature;
int c = 0;
int Temp_Tuning = 2;
int Beat_Tuning = 10;

int wifi_connection = 0;

void setup() {

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  Serial.begin(115200);
  Serial.println("Initializing Serial Communication with baud rate - 115200");
  Serial.println("Trying to connect to wifi");

  for (int a = 0 ; a < 5 ; a++) {
    wifi_connection = ubidots.wifiConnect(WIFI_SSID, WIFI_PASS);
    if (wifi_connection == 1) {
      Serial.println("Wifi Connected Successfully");
      Serial.println(wifi_connection);
      a = 6;
    }
    else {
      Serial.println("Wifi not connected try reseting");
    }
  }

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); //Start the OLED display
  oled.display();
  delay(3000);

  // Initialize sensor
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }

  Serial.println("Place your index finger on the sensor with steady pressure.");
  particleSensor.begin(Wire, I2C_SPEED_FAST);
  particleSensor.setup(); //Configure sensor. Turn off LEDs
  particleSensor.enableDIETEMPRDY();//Enable the temp ready interrupt. This is required.
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.display();
}

void loop() {
  long irValue = particleSensor.getIR(); // IR value using Sensor

  if (irValue > 7000) {
    if (checkForBeat(irValue) == false && c == 0)
    {
      oled.clearDisplay();                                //Clear the display
      oled.setTextSize(1);                                //And still displays the average BPM
      oled.setTextColor(WHITE);

      oled.setCursor(10, 0);
      oled.print("Calculating...");
      oled.display();
      Serial.println("Calculating...");
    }

    if (checkForBeat(irValue) == true && i < no_of_loops) {                     //If a heart beat is detected
      c = 1;
      temperature = particleSensor.readTemperatureF();
      beatAvg = beatAvg + Beat_Tuning;
      temperature = temperature - Temp_Tuning;
      Serial.print("Temp is: ");
      Serial.println(temperature);

      Serial.print("BPM is: ");
      Serial.println(beatAvg);

      oled.clearDisplay();                                //Clear the display
      oled.setTextSize(1);                                //And still displays the average BPM
      oled.setTextColor(WHITE);

      oled.setCursor(10, 0);
      oled.print("Temp:");
      oled.setCursor(50, 0);
      oled.println(temperature);
      oled.setCursor(10, 25);
      oled.print("BPM");
      oled.setCursor(50, 25);
      oled.println(beatAvg);
      oled.display();

      if (beatAvg > lowerlimit && beatAvg < upperlimit) {
        oled.setCursor(10, 50);
        oled.print("STABLE BPM");
        oled.display();
        Serial.println("Stable BPM");
        stable = 1;
        i = i + 1;
        last_value = beatAvg;
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_RED, LOW);
      }

      if ((beatAvg < lowerlimit || beatAvg > upperlimit) && stable == 1 ) {
        oled.setCursor(10, 50);
        oled.print("ALERT!!!");
        oled.display();
        i = i + 1;
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        //delay(1000);
        Serial.println("ALERT!!!");


        if ( last_value < lowerlimit || last_value > upperlimit) {
          alert_count = alert_count + 1;
          Serial.println(alert_count);
        }
        if (last_value > lowerlimit && last_value < upperlimit) {
          alert_count = 0;
        }

        if (alert_count == alert_threshold && wifi_connection == 1) {
          ubidots.add("Alert", beatAvg); //Code toadd value to the variable
          ubidots.add("bpm", beatAvg);
          ubidots.add("temperature", temperature);
          alert_count = 0;
          bool bufferSent = false;
          while (bufferSent == false) {
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_GREEN, LOW);
            bufferSent = ubidots.send("RMS");  // Will send data to a device label that matches the device Id
          }

          Serial.println("Alert Sent!");

          oled.clearDisplay();                                //Clear the display
          oled.setTextSize(1);                                //And still displays the average BPM
          oled.setTextColor(WHITE);
          oled.setCursor(10, 0);
          oled.println("ALERT SENT!!!");
          oled.display();
          delay(1000);
        }

        if (alert_count == alert_threshold && wifi_connection == 0) {
          oled.clearDisplay();                                //Clear the display
          oled.setTextSize(1);                                //And still displays the average BPM
          oled.setTextColor(WHITE);
          oled.setCursor(10, 0);
          oled.println("ALERT SENDING FAILED!");
          oled.setCursor(10, 25);
          oled.println("Wifi Not Connected");
          oled.setCursor(10, 40);
          oled.println("Press Reset");
          oled.display();
          delay(2000);
          i = 0;
        }

        last_value = beatAvg;
      }


      //We sensed a beat!
      long delta = millis() - lastBeat;                   //Measure duration between two beats
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);           //Calculating the BPM

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {              //To calculate the average we strore some values (4) then do some math to calculate the average
        rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
        rateSpot %= RATE_SIZE; //Wrap variable
        //Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    if (i >= no_of_loops && wifi_connection == 1) {
      //stable = 0;
      i = 0;

      oled.clearDisplay();                                //Clear the display
      oled.setTextSize(1);                                //And still displays the average BPM
      oled.setTextColor(WHITE);
      oled.setCursor(10, 0);
      oled.println("Sending Data...");
      oled.setCursor(10, 30);
      //oled.println("BPM reading reset");
      oled.display();
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, LOW);
      Serial.println("Sending Data to Server!");
      ubidots.add("temperature", temperature);
      ubidots.add("bpm", beatAvg);

      bool bufferSent = false;
      while (bufferSent == false) {
        bufferSent = ubidots.send("RMS");  // Will send data to a device label that matches the device Id
        //delay(5000);  // comment this without cloud & uncomment the above statement
        //bufferSent = true; // comment this without cloud & uncomment the above statement
      }
      Serial.println("Data Sent Successfully");
    }

    if (i > no_of_loops && wifi_connection == 0) {
      //stable = 0;
      i = 0;

      oled.clearDisplay();                                //Clear the display
      oled.setTextSize(1);                                //And still displays the average BPM
      oled.setTextColor(WHITE);
      oled.setCursor(10, 0);
      oled.println("Data Sending Failed!");
      oled.setCursor(10, 25);
      oled.println("Wifi Not Connected");
      oled.setCursor(10, 40);
      oled.println("Press Reset");
      oled.display();
      delay(1000);
    }

  }

  if (irValue < 7000) {
    oled.clearDisplay();                                //Clear the display
    oled.setTextSize(1);                                //And still displays the average BPM
    oled.setTextColor(WHITE);
    oled.setCursor(30, 5);
    oled.println("Please place");
    oled.setCursor(30, 15);
    oled.println("your finger");
    oled.display();
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    Serial.println("Finger Not Detected");
    //stable = 0;
    c == 0;
  }

}
