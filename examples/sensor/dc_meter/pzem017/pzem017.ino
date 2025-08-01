// PZEM-017 DC Energy Meter Example
// Compatible with ESP8266 and ESP32

#ifdef ESP8266
#include <ESP8266WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#include <SoftwareSerial.h> // เรียกใช้ไลบรารี SoftwareSerial สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <ModbusMaster.h>   // ModbusMaster by Doc Walker
#include <cynoiot.h> // CynoIOT by IoTbundle

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

#ifdef ESP8266
SoftwareSerial PZEMSerial;
#elif defined(ESP32)
#define PZEMSerial Serial1
#endif

// Address ของ PZEM-017 : 0x01-0xF7
static uint8_t pzemSlaveAddr = 0x01;

// ตั้งค่า shunt -->> 0x0000-100A, 0x0001-50A, 0x0002-200A, 0x0003-300A
static uint16_t NewshuntAddr = 0x0002;

// ตั้งค่า pin สำหรับต่อกับ MAX485
#ifdef ESP8266
#define MAX485_RO D7 // RX
#define MAX485_RE D6
#define MAX485_DE D5
#define MAX485_DI D0 // TX

#elif defined(ESP32)
#define RSTPIN 7

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define MAX485_RO 18
#define MAX485_RE 9
#define MAX485_DE 9
#define MAX485_DI 21

#else
#define MAX485_RO 23
#define MAX485_RE 9
#define MAX485_DE 9
#define MAX485_DI 26
#endif

#endif

ModbusMaster node;
float PZEMVoltage = 0;
float PZEMCurrent = 0;
float PZEMPower = 0;
float PZEMEnergy = 0;

unsigned long previousMillis = 0;
unsigned long startMillis1; // to count time during initial start up
uint8_t numVariables;

void preTransmission()
{
  if (millis() - startMillis1 > 5000) // Wait for 5 seconds as ESP Serial cause start up code crash
  {    
    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, OUTPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    digitalWrite(MAX485_RE, 1); /* put RE Pin to high*/
    digitalWrite(MAX485_DE, 1); /* put DE Pin to high*/
    delay(1);                   // When both RE and DE Pin are high, converter is allow to transmit communication
  }
}

void postTransmission()
{
  if (millis() - startMillis1 > 5000) // Wait for 5 seconds as ESP Serial cause start up code crash
  {
    delay(3);                   // When both RE and DE Pin are low, converter is allow to receive communication
    digitalWrite(MAX485_RE, 0); /* put RE Pin to low*/
    digitalWrite(MAX485_DE, 0); /* put DE Pin to low*/
  }
}

void iotSetup()
{
  numVariables = 4;
  String keyname[numVariables];
  keyname[0] = "v";
  keyname[1] = "i";
  keyname[2] = "p";
  keyname[3] = "e";
  iot.setkeyname(keyname, numVariables);

  Serial.print("Connecting to server.");
  iot.connect(email);
}

void setup()
{
  startMillis1 = millis();
  Serial.begin(115200);

  // Setup RS485 control pins
  pinMode(MAX485_RE, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);

  // Initialize PZEM serial communication
#ifdef ESP8266
  PZEMSerial.begin(9600, SWSERIAL_8N2, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
  PZEMSerial.begin(9600, SERIAL_8N2, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
#endif

  Serial.println();
  Serial.print("Wifi connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Setup ModbusMaster callbacks and begin
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  node.begin(pzemSlaveAddr, PZEMSerial);

  // Wait a moment before setting shunt
  delay(1000);
  Serial.print("Setting PZEM 017 shunt... ");
  setShunt(pzemSlaveAddr);
  Serial.println("done");

  iotSetup();
}

void loop()
{
  iot.handle();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 5000)
  {
    previousMillis = currentMillis;

    uint8_t result;
    result = node.readInputRegisters(0x0000, 6);
    disConnect();
    if (result == node.ku8MBSuccess)
    {
      uint32_t tempdouble = 0x00000000;
      PZEMVoltage = node.getResponseBuffer(0x0000) / 100.0;
      PZEMCurrent = node.getResponseBuffer(0x0001) / 100.0;
      tempdouble = (node.getResponseBuffer(0x0003) << 16) + node.getResponseBuffer(0x0002); // get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit
      PZEMPower = tempdouble / 10.0;                                                        // Divide the value by 10 to get actual power value (as per manual)
      tempdouble = (node.getResponseBuffer(0x0005) << 16) + node.getResponseBuffer(0x0004); // get the energy value. Energy value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit
      PZEMEnergy = tempdouble;
      PZEMEnergy /= 1000; // Convert to kWh

      Serial.print("Vdc : ");
      Serial.print(PZEMVoltage, 1);
      Serial.print(" V   ");
      Serial.print("Idc : ");
      Serial.print(PZEMCurrent, 3);
      Serial.print(" A   ");
      Serial.print("Power : ");
      Serial.print(PZEMPower, 1);
      Serial.print(" W   ");
      Serial.print("Energy : ");
      Serial.print(PZEMEnergy, 3);
      Serial.println(" kWh");

      float val[numVariables] = {PZEMVoltage, PZEMCurrent, PZEMPower, PZEMEnergy};
      iot.update(val);
    }
    else
    {
      Serial.println("Failed to read modbus");
    }
  }
}

void disConnect()
{
  pinMode(MAX485_RE, INPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
  pinMode(MAX485_DE, INPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
}

void setShunt(uint8_t slaveAddr)
{
  static uint8_t SlaveParameter = 0x06;     /* Write command code to PZEM */
  static uint16_t registerAddress = 0x0003; /* change shunt register address command code */

  uint16_t u16CRC = 0xFFFF;                 /* declare CRC check 16 bits*/
  u16CRC = crc16_update(u16CRC, slaveAddr); // Calculate the crc16 over the 6bytes to be send
  u16CRC = crc16_update(u16CRC, SlaveParameter);
  u16CRC = crc16_update(u16CRC, highByte(registerAddress));
  u16CRC = crc16_update(u16CRC, lowByte(registerAddress));
  u16CRC = crc16_update(u16CRC, highByte(NewshuntAddr));
  u16CRC = crc16_update(u16CRC, lowByte(NewshuntAddr));

  preTransmission(); /* trigger transmission mode*/

  PZEMSerial.write(slaveAddr); // these whole process code sequence refer to manual
  PZEMSerial.write(SlaveParameter);
  PZEMSerial.write(highByte(registerAddress));
  PZEMSerial.write(lowByte(registerAddress));
  PZEMSerial.write(highByte(NewshuntAddr));
  PZEMSerial.write(lowByte(NewshuntAddr));
  PZEMSerial.write(lowByte(u16CRC));
  PZEMSerial.write(highByte(u16CRC));
  delay(10);

  postTransmission(); /* trigger reception mode*/
  delay(100);
  disConnect();

  while (PZEMSerial.available())
  {
    Serial.print(char(PZEMSerial.read()), HEX); // Prints the response and display on Serial Monitor (Serial)
    Serial.print(" ");
  }
  Serial.println();
}