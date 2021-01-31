#include <Homie.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_NeoMatrix.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>

#define firmwareVersion "1.0.0"
#define NEO_MATRIX_PIN D5

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Ticker ledMatrixTicker;
Ticker clockTicker;

Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
Adafruit_NeoMatrix matrix   = Adafruit_NeoMatrix(32, 8, NEO_MATRIX_PIN,  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG, NEO_GRB + NEO_KHZ800);
const uint16_t colors[]     = {matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255), matrix.Color(200, 0, 200) };



HomieNode temperatureNode("temperature", "Temperature", "temperature");
HomieNode pressureNode("pressure","Pressure","pressure");
HomieNode altitudeNode("altitude","Altitude","altitude");
HomieNode matrixNode("matrix", "Matrix", "switch");
HomieNode messageNode("message", "Message", "message");

int x                             = matrix.width();
int pass                          = 0;
String message                    = "Welcome";
int mess_len                      = 0;
bool messagePrinted               = false;
bool showDot                      = false;
bool isMatrixOn                   = true;
int tempCountInterval             = 30;
const int TEMPERATURE_INTERVAL    = 300;
unsigned long lastTemperatureSent = 0;
float temperature;
float altitude;

void displayClock() {
  if(messagePrinted) {
    matrix.fillScreen(0);
    matrix.setCursor(2, 0);
    message = "";

    if(timeClient.getHours() <= 9) {
      message += "0";
    }

    message += timeClient.getHours();
    showDot = !showDot;
    if(showDot) {
      message += ":";
    } else {
      message += " ";
    }

    if(timeClient.getMinutes() <= 9) {
      message += "0";
    }
    message += timeClient.getMinutes();
    tempCountInterval--;
    if(tempCountInterval <= 2) {
      message = String(temperature);
      if(tempCountInterval <= 0) {
        tempCountInterval = 30;
      }
    }

    matrix.print(message);
    if(isMatrixOn) matrix.show();
  }
}

bool matrixMessageHandler(const HomieRange& range, const String& value) {
  messageNode.setProperty("state").send("Showing");
  Homie.getLogger() << "Led Matrix message: " << value << endl;
  message = value;
  messagePrinted = false;
  return true;
}

bool matrixOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool on = (value == "true");
  matrixNode.setProperty("on").send(value);
  Homie.getLogger() << "Led Matrix is " << (on ? "on" : "off") << endl;
  isMatrixOn = on;
  if(!isMatrixOn) {
    Serial.println("Deveria apagar tudo");
    matrix.fillScreen(0);
    matrix.clear();
    matrix.show();
    messageNode.setProperty("state").send("Off");
  } else {
    messageNode.setProperty("state").send("Idle");
  }

  return true;
}

void messageScrool() {
  if(!messagePrinted) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);

    mess_len = message.length() * -6;
    matrix.print(message);
    if(--x < mess_len) {
      x = matrix.width();
      if(++pass >= 4) pass = 0;
      matrix.setTextColor(colors[pass]);
      messagePrinted = true;
      messageNode.setProperty("state").send("Idle");
    }

    if(isMatrixOn) matrix.show();
  }
}

void setupHandler() {
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(10);
  matrix.setTextColor(colors[0]);

  ledMatrixTicker.attach_ms(100, messageScrool);
  clockTicker.attach(1,displayClock);

  // NTP
  timeClient.begin();
  timeClient.setTimeOffset(-10800);
  timeClient.forceUpdate();

  // temperature sensor
  if(!bmp.begin()) {
    Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
  }

  temperatureNode.advertise("degrees").setName("Degrees")
                                      .setDatatype("float")
                                      .setUnit("ºC");
  pressureNode.advertise("hpa").setName("hPa")
                                      .setDatatype("float")
                                      .setUnit("hPa");
  altitudeNode.advertise("altitude").setName("Altitude")
                                      .setDatatype("float")
                                      .setUnit("m");

  matrixNode.advertise("on").setName("On").setDatatype("boolean").settable(matrixOnHandler);

  messageNode.advertise("message").setName("Message").setDatatype("string").settable(matrixMessageHandler);
  messageNode.advertise("message").setName("state").setDatatype("string");
}

void loopHandler() {
  if (millis() - lastTemperatureSent >= TEMPERATURE_INTERVAL * 1000UL || lastTemperatureSent == 0) {
    sensors_event_t event;
    bmp.getEvent(&event);
    if (event.pressure) {
      float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;

      bmp.getTemperature(&temperature);
      altitude = bmp.pressureToAltitude(seaLevelPressure, event.pressure);
      Homie.getLogger() << "Temperature: " << temperature << " °C" << endl;
      Homie.getLogger() << "Pressure: " << event.pressure << " hPa" << endl;
      Homie.getLogger() << "Altitude: " << altitude << " m" << endl;
      temperatureNode.setProperty("degrees").send(String(temperature));
      pressureNode.setProperty("hpa").send(String(event.pressure));
      altitudeNode.setProperty("altitude").send(String(altitude));
      lastTemperatureSent = millis();
    }
    else {
      Serial.println("Sensor error");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial << endl << endl;
  Homie_setFirmware("LedMatrixHomieIOT", firmwareVersion);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie.setup();
}

void loop() {
  Homie.loop();
}