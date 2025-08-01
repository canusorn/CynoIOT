#ifdef ESP8266
#include <ESP8266WiFi.h>// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h>// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h> // CynoIOT by IoTbundle

const char ssid[] = "G6PD_2.4G";      // ชื่อ WiFi
const char pass[] = "570610193";      // รหัสผ่าน WiFi
const char email[] = "anusorn1998@gmail.com";  // อีเมลสำหรับลงทะเบียนกับ CynoIOT

Cynoiot iot;  // สร้างอ็อบเจ็กต์ CynoIOT

unsigned long previousMillis = 0;  // ตัวแปรเก็บเวลาล่าสุดที่ส่งข้อมูล

#define RELAY1 D1
#define RELAY2 D2

// ฟังก์ชันตั้งค่าการเชื่อมต่อกับ CynoIOT
void iotSetup()
{
    uint8_t numVariables = 2;  // กำหนดจำนวนตัวแปรที่จะส่งไปยังเซิร์ฟเวอร์
    String keyname[numVariables] = {"charger", "fridge"};  // กำหนดชื่อตัวแปรเป็นความชื้นและอุณหภูมิ
    iot.setkeyname(keyname, numVariables);  // ตั้งค่าชื่อตัวแปรให้กับ CynoIOT
    
    iot.connect(email);  // เชื่อมต่อกับเซิร์ฟเวอร์ด้วยอีเมล
}

// ฟังก์ชันที่ทำงานครั้งแรกเมื่อเริ่มต้นอุปกรณ์
void setup()
{
    Serial.begin(115200);  // เริ่มการสื่อสารผ่าน Serial ที่ความเร็ว 115200 bps
    Serial.println();
    Serial.print("Wifi connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, pass);  // เริ่มการเชื่อมต่อ WiFi ด้วยชื่อและรหัสผ่านที่กำหนด
    while (WiFi.status() != WL_CONNECTED)  // รอจนกว่าจะเชื่อมต่อ WiFi สำเร็จ
    {
        Serial.print(".");
        delay(1000);  // หน่วงเวลา 1 วินาที
    }
    Serial.print("\nWiFi connected, IP address: ");
    Serial.println(WiFi.localIP());  // แสดงหมายเลข IP ที่ได้รับ

    Serial.print("Connecting to server.");
    iotSetup();  // เรียกใช้ฟังก์ชันตั้งค่าการเชื่อมต่อกับ CynoIOT
}

// ฟังก์ชันที่ทำงานวนซ้ำตลอดเวลา
void loop()
{
    iot.handle();  // จัดการการเชื่อมต่อกับ CynoIOT

    unsigned long currentMillis = millis();  // อ่านเวลาปัจจุบันในหน่วยมิลลิวินาที
    if (currentMillis - previousMillis >= 60000)  // ตรวจสอบว่าผ่านไป 60 วินาทีหรือยัง
    {
        previousMillis = currentMillis;  // บันทึกเวลาปัจจุบัน

        float val[2] = {digitalRead(RELAY1), digitalRead(RELAY2)};  // สร้างค่าสุ่มสำหรับความชื้น (70-80) และอุณหภูมิ (20-30)
        iot.update(val);  // ส่งข้อมูลไปยังเซิร์ฟเวอร์
    }
}