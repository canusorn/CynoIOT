// 18 > A
// 21 > B

#include <WiFi.h>
#include <ModbusMaster.h>
#include <cynoiot.h>

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

// Address ของ PZEM-017 : 0x01-0xF7
static uint8_t pzemSlaveAddr = 0x01;

// ตั้งค่า shunt -->> 0x0000-100A, 0x0001-50A, 0x0002-200A, 0x0003-300A
static uint16_t NewshuntAddr = 0x0002;

ModbusMaster node;
float PZEMVoltage = 0;
float PZEMCurrent = 0;
float PZEMPower = 0;
float PZEMEnergy = 0;

unsigned long previousMillis = 0;
uint8_t numVariables;

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
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N2, 18, 21);

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

  setShunt(pzemSlaveAddr);
  node.begin(pzemSlaveAddr, Serial1);

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
    if (result == node.ku8MBSuccess)
    {
      uint32_t tempdouble = 0x00000000;
      PZEMVoltage = node.getResponseBuffer(0x0000) / 100.0;
      PZEMCurrent = node.getResponseBuffer(0x0001) / 100.0;
      tempdouble = (node.getResponseBuffer(0x0003) << 16) + node.getResponseBuffer(0x0002); // get the power value. Power value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit
      PZEMPower = tempdouble / 10.0;                                                        // Divide the value by 10 to get actual power value (as per manual)
      tempdouble = (node.getResponseBuffer(0x0005) << 16) + node.getResponseBuffer(0x0004); // get the energy value. Energy value is consists of 2 parts (2 digits of 16 bits in front and 2 digits of 16 bits at the back) and combine them to an unsigned 32bit
      PZEMEnergy = tempdouble;
      Serial.print(PZEMVoltage, 1); // Print Voltage value on Serial Monitor with 1 decimal*/
      Serial.print("V   ");
      Serial.print(PZEMCurrent, 3);
      Serial.print("A   ");
      Serial.print(PZEMPower, 1);
      Serial.print("W  ");
      Serial.print(PZEMEnergy, 0);
      Serial.print("Wh  ");
      Serial.println();

      float val[numVariables] = {PZEMVoltage, PZEMCurrent, PZEMPower, PZEMEnergy};
      iot.update(val);
    }
    else
    {
      Serial.println("Failed to read modbus");
    }
  }

} // Loop Ends

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

  Serial.println("Change shunt address");
  Serial1.write(slaveAddr); // these whole process code sequence refer to manual
  Serial1.write(SlaveParameter);
  Serial1.write(highByte(registerAddress));
  Serial1.write(lowByte(registerAddress));
  Serial1.write(highByte(NewshuntAddr));
  Serial1.write(lowByte(NewshuntAddr));
  Serial1.write(lowByte(u16CRC));
  Serial1.write(highByte(u16CRC));
  delay(10);
  delay(100);
  while (Serial1.available())
  {
    Serial.print(char(Serial1.read()), HEX); // Prints the response and display on Serial Monitor (Serial)
    Serial.print(" ");
  }
} // setShunt Ends