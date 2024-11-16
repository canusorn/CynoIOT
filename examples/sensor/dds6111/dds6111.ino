/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <ESP8266WiFi.h>
#include <cynoiot.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>

SoftwareSerial RS485Serial;

// ตั้งค่า pin สำหรับต่อกับ MAX485
#define MAX485_RO D7
#define MAX485_RE D6
#define MAX485_DE D5
#define MAX485_DI D0

ModbusMaster node;
Cynoiot iot;

const char ssid[] = "G6PD";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

unsigned long previousMillis;

void setup()
{

    Serial.begin(115200);
    RS485Serial.begin(9600, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485

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
    node.begin(12, RS485Serial);

    uint8_t numVariables = 8;
    String keyname[numVariables] = {"volt", "curr", "power", "pf", "e1", "e2", "e3", "freq"};
    iot.setkeyname(keyname, numVariables);

    Serial.print("Connecting to server.");
    iot.connect(email);
}

void loop()
{

    iot.handle();

    if (!iot.status())
    {
        iot.connect(email);
    }

    // อ่านค่าจาก PZEM-017
    uint32_t currentMillisPZEM = millis();
    if (currentMillisPZEM - previousMillis >= 5000) /* for every x seconds, run the codes below*/
    {
        uint8_t result;                               /* Declare variable "result" as 8 bits */
        result = node.readInputRegisters(0x0000, 16); /* read the 9 registers (information) of the PZEM-014 / 016 starting 0x0000 (voltage information) kindly refer to manual)*/
        if (result == node.ku8MBSuccess)              /* If there is a response */
        {
            uint32_t tempdouble = 0x00000000; /* Declare variable "tempdouble" as 32 bits with initial value is 0 */
            uint32_t var[8];

            tempdouble = (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[0] = tempdouble;                                                        /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(2) << 16) + node.getResponseBuffer(3); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[1] = tempdouble;                                                        /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(4) << 16) + node.getResponseBuffer(5); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[2] = tempdouble;                                                        /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(6) << 16) + node.getResponseBuffer(7); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[3] = tempdouble;                                                        /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(8) << 16) + node.getResponseBuffer(9); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[4] = tempdouble;                                                        /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(10) << 16) + node.getResponseBuffer(11); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[5] = tempdouble;                                                          /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(12) << 16) + node.getResponseBuffer(13); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[6] = tempdouble;                                                          /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(14) << 16) + node.getResponseBuffer(15); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[7] = tempdouble;                                                          /* Divide the value by 10 to get actual power value (as per manual) */

            // แสดงค่าที่ได้จากบน Serial monitor
            float varfloat[8];

            varfloat[0] = hexToFloat(var[0]);
            varfloat[1] = hexToFloat(var[1]);
            varfloat[2] = hexToFloat(var[2]);
            varfloat[3] = hexToFloat(var[3]);
            varfloat[4] = hexToFloat(var[4]);
            varfloat[5] = hexToFloat(var[5]);
            varfloat[6] = hexToFloat(var[6]);
            varfloat[7] = hexToFloat(var[7]);

            Serial.println("0x" + String(var[0], HEX) + "\t" + String(varfloat[0], 6) + " v");
            Serial.println("0x" + String(var[1], HEX) + "\t" + String(varfloat[1], 6) + " a");
            Serial.println("0x" + String(var[2], HEX) + "\t" + String(varfloat[2], 6) + " w");
            Serial.println("0x" + String(var[3], HEX) + "\t" + String(varfloat[3], 6) + " pf");
            Serial.println("0x" + String(var[4], HEX) + "\t" + String(varfloat[4], 6) + " Wh");
            Serial.println("0x" + String(var[5], HEX) + "\t" + String(varfloat[5], 6) + " Wh");
            Serial.println("0x" + String(var[6], HEX) + "\t" + String(varfloat[6], 6) + " Wh");
            Serial.println("0x" + String(var[7], HEX) + "\t" + String(varfloat[7], 6) + " Hz");
            Serial.println("------------");

            iot.update(varfloat);
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

float hexToFloat(uint32_t hex_value)
{
    union
    {
        uint32_t i;
        float f;
    } u;

    u.i = hex_value;
    return u.f;
}