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
    node.begin(1, RS485Serial);

    uint8_t numVariables = 7 * 3;
    String keyname[numVariables] = {
        "v1", "i1", "p1", "q1", "pf1", "f1", "e1",
        "v2", "i2", "p2", "q2", "pf2", "f2", "e2",
        "v3", "i3", "p3", "q3", "pf3", "f3", "e3"};
    iot.setkeyname(keyname, numVariables);

    Serial.print("Connecting to server.");
    iot.connect(email);
}

void loop()
{
    iot.handle();

    uint32_t currentMillisPZEM = millis();
    if (currentMillisPZEM - previousMillis >= 5000) /* for every x seconds, run the codes below*/
    {
        bool isError = false;
        float varfloat[7 * 3];
        uint32_t tempdouble = 0x00000000; /* Declare variable "tempdouble" as 32 bits with initial value is 0 */
        uint32_t var[7 * 3];
        uint8_t result;                               /* Declare variable "result" as 8 bits */
        result = node.readInputRegisters(0x2006, 47); /* read the 9 registers (information) of the PZEM-014 / 016 starting 0x0000 (voltage information) kindly refer to manual)*/
        if (result == node.ku8MBSuccess)              /* If there is a response */
        {
            // voltage
            tempdouble = (node.getResponseBuffer(0x2006) << 16) + node.getResponseBuffer(0x2007); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[0] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2008) << 16) + node.getResponseBuffer(0x2009); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[1] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x200A) << 16) + node.getResponseBuffer(0x200B); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[2] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            // current
            tempdouble = (node.getResponseBuffer(0x200C) << 16) + node.getResponseBuffer(0x200D); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[3] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x200E) << 16) + node.getResponseBuffer(0x200F); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[4] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2010) << 16) + node.getResponseBuffer(0x2011); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[5] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            // power
            tempdouble = (node.getResponseBuffer(0x2014) << 16) + node.getResponseBuffer(0x2015); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[6] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2016) << 16) + node.getResponseBuffer(0x2017); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[7] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2018) << 16) + node.getResponseBuffer(0x2019); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[8] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            // pf
            tempdouble = (node.getResponseBuffer(0x202C) << 16) + node.getResponseBuffer(0x202D); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[9] = tempdouble;                                                                  /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x202E) << 16) + node.getResponseBuffer(0x202F); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[10] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2030) << 16) + node.getResponseBuffer(0x2031); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[11] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            // pf
            tempdouble = (node.getResponseBuffer(0x202C) << 16) + node.getResponseBuffer(0x202D); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[12] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x202E) << 16) + node.getResponseBuffer(0x202F); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[13] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            tempdouble = (node.getResponseBuffer(0x2030) << 16) + node.getResponseBuffer(0x2031); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[14] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            // freq
            tempdouble = (node.getResponseBuffer(0x2030) << 16) + node.getResponseBuffer(0x2031); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[15] = tempdouble;                                                                 /* Divide the value by 10 to get actual power value (as per manual) */

            // แสดงค่าที่ได้จากบน Serial monitor

            varfloat[0] = hexToFloat(var[0]);
            varfloat[1] = hexToFloat(var[1]);
            varfloat[2] = hexToFloat(var[2]) * 1000;
            varfloat[3] = hexToFloat(var[3]) * 1000;
            varfloat[4] = hexToFloat(var[4]);
            varfloat[5] = hexToFloat(var[5]);
            // varfloat[6] = hexToFloat(var[6]);

            Serial.println("0x" + String(var[0], HEX) + "\t" + String(varfloat[0], 6) + " v");
            Serial.println("0x" + String(var[1], HEX) + "\t" + String(varfloat[1], 6) + " a");
            Serial.println("0x" + String(var[2], HEX) + "\t" + String(varfloat[2], 6) + " w");
            Serial.println("0x" + String(var[3], HEX) + "\t" + String(varfloat[3], 6) + " var");
            Serial.println("0x" + String(var[4], HEX) + "\t" + String(varfloat[4], 6) + " pf");
            Serial.println("0x" + String(var[5], HEX) + "\t" + String(varfloat[5], 6) + " Hz");
        }
        else
        {
            isError = true;
            Serial.println("Error 0x2000 reading");
        }
        result = node.readInputRegisters(0x4000, 2);
        if (result == node.ku8MBSuccess) /* If there is a response */
        {
            tempdouble = (node.getResponseBuffer(0x4000) << 16) + node.getResponseBuffer(0x4001); /* get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit */
            var[6] = tempdouble;

            varfloat[6] = hexToFloat(var[6]);

            Serial.println("0x" + String(var[6], HEX) + "\t" + String(varfloat[6], 6) + " kWh");
            Serial.println("------------");
        }
        else
        {
            isError = true;
            Serial.println("Error 0x4000 reading");
        }

        if (!isError)
        {
            iot.update(varfloat);
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