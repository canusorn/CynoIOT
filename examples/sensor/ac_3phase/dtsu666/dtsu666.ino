/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

#include <WiFi.h>         // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#include <cynoiot.h>      // CynoIOT by IoTbundle
#include <ModbusMaster.h> // ModbusMaster by Doc Walker

#define RS485Serial Serial1

#define ADDR 0x01
#define CTRATIO 60

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

// ตั้งค่า pin สำหรับต่อกับ MAX485
#define MAX485_RO 18
#define MAX485_RE 9
#define MAX485_DE 9
#define MAX485_DI 21

ModbusMaster node;
Cynoiot iot;

unsigned long previousMillis;

void setup()
{

    Serial.begin(115200);
    RS485Serial.begin(9600, SERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485

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

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(1, RS485Serial);

    uint8_t numVariables = 21;
    String keyname[numVariables] = {
        "Va", "Vb", "Vc",
        "Ia", "Ib", "Ic",
        "Pa", "Pb", "Pc",
        "Pfa", "Pfb", "Pfc",
        "F",
        "E1", "E2", "E3", "E4", "E5",
        "E6", "E7", "E8"};
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
        float varfloat[21];
        uint32_t tempdouble = 0x00000000; /* Declare variable "tempdouble" as 32 bits with initial value is 0 */
        uint32_t var[21];
        uint8_t result;                               /* Declare variable "result" as 8 bits */
        result = node.readInputRegisters(0x2006, 64); /* read the 9 registers (information) of the PZEM-014 / 016 starting 0x0000 (voltage information) kindly refer to manual)*/
        if (result == node.ku8MBSuccess)              /* If there is a response */
        {
            // voltage
            tempdouble = (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);
            var[0] = tempdouble;

            tempdouble = (node.getResponseBuffer(2) << 16) + node.getResponseBuffer(3);
            var[1] = tempdouble;

            tempdouble = (node.getResponseBuffer(4) << 16) + node.getResponseBuffer(5);
            var[2] = tempdouble;

            // current
            tempdouble = (node.getResponseBuffer(6) << 16) + node.getResponseBuffer(7);
            var[3] = tempdouble;

            tempdouble = (node.getResponseBuffer(8) << 16) + node.getResponseBuffer(9);
            var[4] = tempdouble;

            tempdouble = (node.getResponseBuffer(10) << 16) + node.getResponseBuffer(11);
            var[5] = tempdouble;

            // power
            tempdouble = (node.getResponseBuffer(14) << 16) + node.getResponseBuffer(15);
            var[6] = tempdouble;

            tempdouble = (node.getResponseBuffer(16) << 16) + node.getResponseBuffer(17);
            var[7] = tempdouble;

            tempdouble = (node.getResponseBuffer(18) << 16) + node.getResponseBuffer(19);
            var[8] = tempdouble;

            // pf
            tempdouble = (node.getResponseBuffer(38) << 16) + node.getResponseBuffer(39);
            var[9] = tempdouble;

            tempdouble = (node.getResponseBuffer(40) << 16) + node.getResponseBuffer(41);
            var[10] = tempdouble;

            tempdouble = (node.getResponseBuffer(42) << 16) + node.getResponseBuffer(43);
            var[11] = tempdouble;

            // freq
            tempdouble = (node.getResponseBuffer(62) << 16) + node.getResponseBuffer(63);
            var[12] = tempdouble;

            // แสดงค่าที่ได้จากบน Serial monitor
            // V
            varfloat[0] = hexToFloat(var[0]) * 0.1;
            varfloat[1] = hexToFloat(var[1]) * 0.1;
            varfloat[2] = hexToFloat(var[2]) * 0.1;
            // I
            varfloat[3] = hexToFloat(var[3]) * 0.001*CTRATIO;
            varfloat[4] = hexToFloat(var[4]) * 0.001*CTRATIO;
            varfloat[5] = hexToFloat(var[5]) * 0.001*CTRATIO;
            // P to kW
            varfloat[6] = hexToFloat(var[6]) * 0.1 * 0.001*CTRATIO;
            varfloat[7] = hexToFloat(var[7]) * 0.1 * 0.001*CTRATIO;
            varfloat[8] = hexToFloat(var[8]) * 0.1 * 0.001*CTRATIO;
            // pf
            varfloat[9] = hexToFloat(var[9]) * 0.001;
            varfloat[10] = hexToFloat(var[10]) * 0.001;
            varfloat[11] = hexToFloat(var[11]) * 0.001;
            // Freq
            varfloat[12] = hexToFloat(var[12]) * 0.01;

            Serial.println("Va\t" + String(varfloat[0], 3) + " v");
            Serial.println("Vb\t" + String(varfloat[1], 3) + " v");
            Serial.println("Vc\t" + String(varfloat[2], 3) + " v");

            Serial.println("Ia\t" + String(varfloat[3], 3) + " A");
            Serial.println("Ib\t" + String(varfloat[4], 3) + " A");
            Serial.println("Ic\t" + String(varfloat[5], 3) + " A");

            Serial.println("Pa\t" + String(varfloat[6], 3) + " kW");
            Serial.println("Pb\t" + String(varfloat[7], 3) + " kW");
            Serial.println("Pc\t" + String(varfloat[8], 3) + " kW");

            Serial.println("Pf1\t" + String(varfloat[9], 3));
            Serial.println("Pf2\t" + String(varfloat[10], 3));
            Serial.println("Pf3\t" + String(varfloat[11], 3));

            Serial.println("F\t" + String(varfloat[12], 3) + " Hz");
        }
        else
        {
            isError = true;
            Serial.println("Error 0x2000 reading");
        }

        result = node.readInputRegisters(0x4101E, 20);
        disConnect();
        if (result == node.ku8MBSuccess) /* If there is a response */
        {
            // ImpEnergy
            tempdouble = (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);
            var[13] = tempdouble;
            tempdouble = (node.getResponseBuffer(2) << 16) + node.getResponseBuffer(3);
            var[14] = tempdouble;
            tempdouble = (node.getResponseBuffer(4) << 16) + node.getResponseBuffer(5);
            var[15] = tempdouble;
            tempdouble = (node.getResponseBuffer(6) << 16) + node.getResponseBuffer(7);
            var[16] = tempdouble;

            // ExpEnergy
            tempdouble = (node.getResponseBuffer(10) << 16) + node.getResponseBuffer(11);
            var[17] = tempdouble;
            tempdouble = (node.getResponseBuffer(12) << 16) + node.getResponseBuffer(13);
            var[18] = tempdouble;
            tempdouble = (node.getResponseBuffer(14) << 16) + node.getResponseBuffer(15);
            var[19] = tempdouble;
            tempdouble = (node.getResponseBuffer(16) << 16) + node.getResponseBuffer(17);
            var[20] = tempdouble;

            varfloat[13] = hexToFloat(var[13]) * CTRATIO;
            varfloat[14] = hexToFloat(var[14]) * CTRATIO;
            varfloat[15] = hexToFloat(var[15]) * CTRATIO;
            varfloat[16] = hexToFloat(var[16]) * CTRATIO;
            varfloat[17] = hexToFloat(var[17]) * CTRATIO;
            varfloat[18] = hexToFloat(var[18]) * CTRATIO;
            varfloat[19] = hexToFloat(var[19]) * CTRATIO;
            varfloat[20] = hexToFloat(var[20]) * CTRATIO;

            Serial.println("ImpEp\t" + String(varfloat[13], 3) + " kWh");
            Serial.println("ImpEp A\t" + String(varfloat[14], 3) + " kWh");
            Serial.println("ImpEp B\t" + String(varfloat[15], 3) + " kWh");
            Serial.println("ImpEp C\t" + String(varfloat[16], 3) + " kWh");

            Serial.println("ExpEp\t" + String(varfloat[17], 3) + " kWh");
            Serial.println("ExpEp A\t" + String(varfloat[18], 3) + " kWh");
            Serial.println("ExpEp B\t" + String(varfloat[19], 3) + " kWh");
            Serial.println("ExpEp C\t" + String(varfloat[20], 3) + " kWh");
            Serial.println("---------------------------------------");
        }
        else
        {
            isError = true;
            Serial.println("Error Energy reading");
        }

        if (!isError)
        {
            iot.update(varfloat);
        }
        else
        {
            iot.debug("Error sensor reading");
        }

        previousMillis = currentMillisPZEM; /* Set the starting point again for next counting time */
    }
}

void preTransmission() /* transmission program when triggered*/
{
    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, OUTPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/

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

void disConnect()
{
    pinMode(MAX485_RE, INPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, INPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
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