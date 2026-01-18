/* ------------------------------------------------------------------------
 * Combined Safety Taser System
 * GSM (SIM800L) + GPS (NEO-6M) + Emergency Button + Analog Sensor
 * ------------------------------------------------------------------------*/

#include <SoftwareSerial.h>
#include <TinyGPS++.h>

/* ================= GSM MODULE ================= */
#define rxPin 2
#define txPin 3
SoftwareSerial sim800L(rxPin, txPin);

/* ================= GPS MODULE ================= */
SoftwareSerial gpsSerial(8, 9);
TinyGPSPlus gps;

/* ================= EMERGENCY BUTTON ================= */
#define EMERGENCY_BUTTON 5

/* ================= ANALOG SENSOR ================= */
#define SENSOR_PIN A0
#define SENSOR_OUTPUT_LED 13
int sensor;

/* ================= GPS VARIABLES ================= */
float latitude = 0.0;
float longitude = 0.0;
bool gpsValid = false;
unsigned long lastGpsPrint = 0;
const unsigned long GPS_PRINT_INTERVAL = 5000;

/* ================= BUTTON DEBOUNCE ================= */
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

/* ================= EMERGENCY COOLDOWN ================= */
bool emergencyTriggered = false;
unsigned long emergencyTriggerTime = 0;
const unsigned long TRIGGER_COOLDOWN = 5000;

String buff;

/* =================================================== */
void setup()
{
  Serial.begin(9600);
  sim800L.begin(9600);
  gpsSerial.begin(9600);

  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
  pinMode(SENSOR_OUTPUT_LED, OUTPUT);

  Serial.println("Initializing Safety Taser System...");

  /* GSM Init */
  sim800L.println("AT");
  waitForResponse();
  sim800L.println("ATE1");
  waitForResponse();
  sim800L.println("AT+CMGF=1");
  waitForResponse();
  sim800L.println("AT+CNMI=1,2,0,0,0");
  waitForResponse();

  Serial.println("System Ready!");
}

/* =================================================== */
void loop()
{
  /* ---------- GPS READ ---------- */
  while (gpsSerial.available())
  {
    int data = gpsSerial.read();
    if (gps.encode(data))
    {
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        gpsValid = true;

        if (millis() - lastGpsPrint > GPS_PRINT_INTERVAL)
        {
          Serial.print("GPS: ");
          Serial.print(latitude, 6);
          Serial.print(", ");
          Serial.println(longitude, 6);
          lastGpsPrint = millis();
        }
      }
    }
  }

  /* ---------- EMERGENCY BUTTON ---------- */
  int reading = digitalRead(EMERGENCY_BUTTON);

  if (reading != lastButtonState)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {
    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == LOW)
      {
        if (!emergencyTriggered ||
            millis() - emergencyTriggerTime >= TRIGGER_COOLDOWN)
        {
          triggerEmergency();
          emergencyTriggered = true;
          emergencyTriggerTime = millis();
        }
      }
    }
  }
  lastButtonState = reading;

  if (emergencyTriggered &&
      millis() - emergencyTriggerTime >= TRIGGER_COOLDOWN)
    emergencyTriggered = false;

  /* ---------- ANALOG SENSOR ---------- */
  sensor = analogRead(SENSOR_PIN);
  Serial.print("Sensor Value: ");
  Serial.println(sensor);

  if (sensor > 495)
    digitalWrite(SENSOR_OUTPUT_LED, HIGH);
  else
    digitalWrite(SENSOR_OUTPUT_LED, LOW);

  /* ---------- GSM RESPONSE ---------- */
  while (sim800L.available())
    Serial.println(sim800L.readString());

  /* ---------- SERIAL COMMANDS ---------- */
  while (Serial.available())
  {
    buff = Serial.readString();
    buff.trim();

    if (buff == "s") send_sms();
    else if (buff == "c") make_call();
    else sim800L.println(buff);
  }
}

/* =================================================== */
void triggerEmergency()
{
  Serial.println("!!! EMERGENCY ACTIVATED !!!");
  send_sms();
  delay(2000);
  make_call();
}

/* =================================================== */
void send_sms()
{
  sim800L.print("AT+CMGS=\"+917306677680\"\r");
  waitForResponse();

  String message = "NIHAL HAS ACTIVATED THE TASER!\n\nLOCATION:\n";

  if (gpsValid)
  {
    message += "Lat: " + String(latitude, 6);
    message += "\nLng: " + String(longitude, 6);
  }
  else
  {
    message += "GPS FIX NOT AVAILABLE";
  }

  sim800L.print(message);
  sim800L.write(0x1A);
  waitForResponse();
}

/* =================================================== */
void make_call()
{
  sim800L.println("ATD+917306677680;");
  waitForResponse();
}

/* =================================================== */
void waitForResponse()
{
  delay(1000);
  while (sim800L.available())
    Serial.println(sim800L.readString());
}
