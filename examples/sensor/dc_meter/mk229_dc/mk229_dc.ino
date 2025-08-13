/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
#ifdef ESP8266
#include <ESP8266WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#include <SoftwareSerial.h> // เรียกใช้ไลบรารี SoftwareSerial สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h> // CynoIOT by IoTbundle 
#include <ModbusMaster.h>   // ModbusMaster by Doc Walker

#ifdef ESP8266
SoftwareSerial RS485Serial;
#elif defined(ESP32)
#define RS485Serial Serial1
#endif

// ตั้งค่า pin สำหรับต่อกับ MAX485
#ifdef ESP8266
#define MAX485_RO D7
#define MAX485_RE D6
#define MAX485_DE D5
#define MAX485_DI D0

#elif defined(ESP32)
#define MAX485_RO 11
#define MAX485_RE 9
#define MAX485_DE 7
#define MAX485_DI 5
#endif

ModbusMaster node;
Cynoiot iot;

// Address  0x01-0xF7
static uint8_t slaveAddr = 0x01;

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

unsigned long previousMillis;

void setup()
{

    Serial.begin(115200);

#ifdef ESP8266
    RS485Serial.begin(9600, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    RS485Serial.begin(9600, SERIAL_8N1, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
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

    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, OUTPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    digitalWrite(MAX485_RE, 0); /* Arduino create output signal for pin RE as LOW (no output)*/
    digitalWrite(MAX485_DE, 0); /* Arduino create output signal for pin DE as LOW (no output)*/

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(slaveAddr, RS485Serial);

    // changeAddress(2);

    uint8_t numVariables = 4;
    String keyname[numVariables] = {"v", "i", "p", "e"};
    iot.setkeyname(keyname, numVariables);

    Serial.print("Connecting to server.");
    iot.connect(email);
}

void loop()
{
    iot.handle();

    // อ่านค่าจาก PZEM-017
    uint32_t currentMillisPZEM = millis();
    if (currentMillisPZEM - previousMillis >= 5000) /* for every x seconds, run the codes below*/
    {
        uint8_t result;                                /* Declare variable "result" as 8 bits */
        result = node.readHoldingRegisters(0x0100, 21); /* read the 9 registers (information) of the PZEM-014 / 016 starting 0x0000 (voltage information) kindly refer to manual)*/
        if (result == node.ku8MBSuccess)               /* If there is a response */
        {
            uint32_t tempdouble = 0x00000000; /* Declare variable "tempdouble" as 32 bits with initial value is 0 */
            float var[4];

            tempdouble = (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[0] = float(tempdouble) / 10000;                                         /* Divide the value by 10 to get actual power value (as per manual) */
            Voltage = var[0];

            tempdouble = (node.getResponseBuffer(2) << 16) + node.getResponseBuffer(3); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[1] = float(tempdouble) / 10000;                                         /* Divide the value by 10 to get actual power value (as per manual) */
            Current = var[1];

            tempdouble = (node.getResponseBuffer(4) << 16) + node.getResponseBuffer(5); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[2] = float(tempdouble) / 10000;                                         /* Divide the value by 10 to get actual power value (as per manual) */
            Power = var[2];

            tempdouble = (node.getResponseBuffer(0xE) << 16) + node.getResponseBuffer(0xF); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[3] = float(tempdouble) / 1000;                                          /* Divide the value by 10 to get actual power value (as per manual) */
            Energy = var[3];

            tempdouble =  node.getResponseBuffer(0x14); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            if(tempdouble){
              var[1]*=-1.0;
              var[2]*=-1.0;
              Current = var[1];
              Power = var[2];
            }
            
            Serial.println(String(var[0], 2) + " V");
            Serial.println(String(var[1], 2) + " A");
            Serial.println(String(var[2], 2) + " W");
            Serial.println(String(var[3], 3) + " kWh");
            Serial.println("------------");

            iot.update(var);
        }
        else
        {
            Serial.println("error");
        }

        previousMillis = currentMillisPZEM; /* Set the starting point again for next counting time */
    }
}

void preTransmission() /* transmission program when triggered*/
{

    digitalWrite(MAX485_RE, 1); /* put RE Pin to high*/
    digitalWrite(MAX485_DE, 1); /* put DE Pin to high*/
    delay(1);                   // When both RE and DE Pin are high, converter is allow to transmit communication
}

void postTransmission() /* Reception program when triggered*/
{

    delay(3);                   // When both RE and DE Pin are low, converter is allow to receive communication
    digitalWrite(MAX485_RE, 0); /* put RE Pin to low*/
    digitalWrite(MAX485_DE, 0); /* put DE Pin to low*/
}

// change address but not test yet
void changeAddress(uint8_t NewslaveAddr)
{
    uint8_t result;

    uint16_t writeData = (NewslaveAddr << 8) + 0x06;

    result = node.writeMultipleRegisters(0x0004, writeData);

    // Check the result of the Modbus operation
    if (result == node.ku8MBSuccess)
    {
        Serial.println("Write Success!");
    }
    else
    {
        Serial.print("Write Failed. Error: ");
        Serial.println(result, HEX);
    }
}