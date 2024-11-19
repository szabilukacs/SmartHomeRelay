#define BLYNK_PRINT Serial

char ssid[] = ""; // your wifi credentials
char pass[] = ""; // your wifi password

// Your Blynk credentials
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
char auth[] = "";

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 27   // PIN for temperature sensor

#define RELAY_PIN 13      // Relay control PIN

// Onewire init
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

SimpleTimer timer;
DeviceAddress tempDeviceAddress; // Temporary address of the found device

float volatile temperature[2];  // We can store maximum 2 temperature value from 2 sensor
int sensorValue = 0;
int temperature_ref = 0;        // Reference temperature value for termostat function

// Blynk PINs
// Blynk virtual pins connected to the button widget

// Relay switch ON/OFF
#define VPIN_BUTTON V0
// temperature1
#define VPIN_TEMP V1
// Reference temperature control
#define VPIN_TEMP_REF V2
// Notification ON/OFF
#define VPIN_SWITCH_NOTIFY V3
// Automata Mod ON/OFF
#define VPIN_BUTTON_AUTOMATA V4
// (optional) temperature2
#define VPIN_TEMP2 V5
// Safety notification, max temperature of water
#define WATER_HOT_VALUE 70

// Number of temperature devices found
int numberOfDevices;

bool notifications_on = false;

bool auto_mod = false;

void setup() {

  // Debug console
  Serial.begin(115200);
  // Initialize the relay pin as an output
  pinMode(RELAY_PIN, OUTPUT);
  // Start with relay off
  digitalWrite(RELAY_PIN, LOW);
  // Connect to Wi-Fi and Blynk
  Blynk.begin(auth, ssid, pass);

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  // Init the temperature sensors
  init_temperature_sensor();

  timer.setInterval(5000L, sendTemps);    // Read temperature sensor data every 5 minutes

}

// Switch notifications ON/OFF
BLYNK_WRITE(VPIN_SWITCH_NOTIFY) {
  int buttonState = param.asInt(); // Get the value from the widget (0 or 1)

  if (buttonState == 1) {
    notifications_on = true;
  } else {
    notifications_on = false;
  }
}

// Set reference temperature
BLYNK_WRITE(VPIN_TEMP_REF) {
  temperature_ref = param.asInt(); // Get the value from the widget
}

// Switch Automatic mode ON/OFF
BLYNK_WRITE(VPIN_BUTTON_AUTOMATA) {
  int buttonState = param.asInt(); // Get the value from the widget (0 or 1)

  if (buttonState == 1) {
    auto_mod = true;
  } else {
    auto_mod = false;
  }
}

// Switch relay manually
BLYNK_WRITE(VPIN_BUTTON) {
  int buttonState = param.asInt(); // Get the value from the widget (0 or 1)

  if (buttonState == 1) {
    digitalWrite(RELAY_PIN, HIGH);  // Turn on relay
    Serial.println("Relay is ON");
  } else {
    digitalWrite(RELAY_PIN, LOW);   // Turn off relay
    Serial.println("Relay is OFF");
  }
}

void loop() {
  Blynk.run();
  timer.run();
}

void sendTemps()
{
  // Get data from the sensor
  measure_temperature();

  // Filter the -127 value, initializing again
  if ((temperature[0] < -126) && (temperature[0] > -128))
  {
    init_temperature_sensor();
  }
  else
  {
    if (numberOfDevices == 0)
    {
      Serial.println("No device connected!");
      Blynk.logEvent("nincsmeres", String("Hibás mérés, a hőmérsékletmérő nem működik!"));
    }
    if (numberOfDevices == 1)
    {
      Blynk.virtualWrite(VPIN_TEMP, temperature[0]);         // Send temperature to Blynk app virtual pin 1.
    }
    if (numberOfDevices == 2)
    {
      Blynk.virtualWrite(VPIN_TEMP, temperature[0]);         // Send temperature to Blynk app virtual pin 1.
      Blynk.virtualWrite(VPIN_TEMP2, temperature[1]);         // Send temperature to Blynk app virtual pin 2.
    }
  }

  // If Automata Mode is activated turn off the relay if we reached the reference temperature
  if (auto_mod)
  {
    if (temperature[0] >= temperature_ref)
    {
      digitalWrite(RELAY_PIN, LOW);   // Turn off relay
      Blynk.virtualWrite(VPIN_BUTTON, 0);   // switch to 0 virtual pin also
    }
  }

  // If notifications is ON send events for setted references and turn off relay
  if (notifications_on)
  {
    if (temperature[0] >= WATER_HOT_VALUE)
    {
      Blynk.logEvent("tulmelegedes", String("A viz hőmérséklete több mint: ") + WATER_HOT_VALUE + String("A fűtés kikapcsolt!"));
      digitalWrite(RELAY_PIN, LOW);   // Turn off relay
    }

    if (temperature[0] >= temperature_ref)
    {
      Blynk.logEvent("vizjo", String("A viz hőmérséklete elérte a beállított hőfokot!") + temperature[0]);
    }
  }
}

// Get temperature from sensor in Celsius
void measure_temperature()
{
  // Temperatures
  sensors.requestTemperatures(); // Send the command to get temperatures

  if (numberOfDevices == 1)
  {
    temperature[0] = sensors.getTempCByIndex(0);
  }
  if (numberOfDevices == 2)
  {
    temperature[0] = sensors.getTempCByIndex(0);
    temperature[1] = sensors.getTempCByIndex(1);
  }
}

// Initialize temperature sensors
void init_temperature_sensor()
{
  sensors.begin(); // Start up the library
  numberOfDevices = sensors.getDeviceCount(); // Grab a count of devices on the wire

  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
      // set the resolution to 11 bit (Each Dallas/Maxim device is capable of several different resolutions)
      sensors.setResolution(tempDeviceAddress, 11);
    }
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
