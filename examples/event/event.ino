#ifdef ESP8266
#include <ESP8266WiFi.h>
// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h>
// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h>
// เรียกใช้ไลบรารี CynoIOT สำหรับเชื่อมต่อกับเซิร์ฟเวอร์

#define LED 15  // กำหนดขาที่ต่อกับ LED เป็นขา 15

const char ssid[] = "G6PD_2.4G";      // ชื่อ WiFi
const char pass[] = "570610193";      // รหัสผ่าน WiFi
const char email[] = "anusorn1998@gmail.com";  // อีเมลสำหรับลงทะเบียนกับ CynoIOT

Cynoiot iot;  // สร้างอ็อบเจ็กต์ CynoIOT

unsigned long previousMillis = 0;  // ตัวแปรเก็บเวลาล่าสุดที่ส่งข้อมูล
bool ledState = 0;  // ตัวแปรเก็บสถานะของ LED (0 = ปิด, 1 = เปิด)

// ฟังก์ชันสำหรับรับ event จากเซิร์ฟเวอร์
void handleEvent(String event, String value)
{
    Serial.println("Event: " + event);  // แสดงชื่อ event ที่ได้รับ
    Serial.println("Value: " + value);  // แสดงค่าของ event ที่ได้รับ

    if (event == "led")     // ตรวจสอบว่าเป็น event ชื่อ "led" หรือไม่
    {
        ledState = value.toInt();     // แปลงค่า value เป็น int แล้วเก็บไว้ใน ledState
        digitalWrite(LED, ledState);  // เปิด/ปิด LED จากค่า value
    }
}

// ฟังก์ชันตั้งค่าการเชื่อมต่อกับ CynoIOT
void iotSetup()
{
    uint8_t numVariables = 2;  // กำหนดจำนวนตัวแปรที่จะส่งไปยังเซิร์ฟเวอร์
    String keyname[numVariables] = {"humid", "temp"};  // กำหนดชื่อตัวแปรเป็นความชื้นและอุณหภูมิ
    iot.setkeyname(keyname, numVariables);  // ตั้งค่าชื่อตัวแปรให้กับ CynoIOT
    
    iot.connect(email);  // เชื่อมต่อกับเซิร์ฟเวอร์ด้วยอีเมล
}

// ฟังก์ชันที่ทำงานครั้งแรกเมื่อเริ่มต้นอุปกรณ์
void setup()
{
    pinMode(LED, OUTPUT);  // กำหนดให้ขา LED เป็นเอาต์พุต

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
    iot.setEventCallback(handleEvent);  // ตั้งค่าฟังก์ชัน callback สำหรับรับ event จากเซิร์ฟเวอร์
}

// ฟังก์ชันที่ทำงานวนซ้ำตลอดเวลา
void loop()
{
    iot.handle();  // จัดการการเชื่อมต่อกับ CynoIOT

    unsigned long currentMillis = millis();  // อ่านเวลาปัจจุบันในหน่วยมิลลิวินาที
    if (currentMillis - previousMillis >= 5000)  // ตรวจสอบว่าผ่านไป 5 วินาทีหรือยัง
    {
        previousMillis = currentMillis;  // บันทึกเวลาปัจจุบัน

        float val[2] = {random(70, 80), random(20, 30)};  // สร้างค่าสุ่มสำหรับความชื้น (70-80) และอุณหภูมิ (20-30)
        iot.update(val);  // ส่งข้อมูลไปยังเซิร์ฟเวอร์

        ledState = !ledState;                  // สลับสถานะ LED
        digitalWrite(LED, ledState);           // เปิด/ปิด LED
        iot.eventUpdate("led", ledState);      // อัพเดท event ไปยัง server
    }
}