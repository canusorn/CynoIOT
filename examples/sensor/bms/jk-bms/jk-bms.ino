#include <NimBLEDevice.h>

// ================= USER SETTINGS =================
static BLEAddress bmsAddress("C8:47:80:14:CB:3B", BLE_ADDR_PUBLIC); 
// =================================================

static BLEUUID serviceUUID("FFE0");
static BLEUUID charUUID("FFE1");

BLERemoteCharacteristic* pRemoteCharacteristic;
bool connected = false;

// The standard request command for JK BMS
uint8_t requestCmd[] = {0xAA, 0x55, 0x90, 0xEB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11};

// Data Parser
void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 20) return; // Ignore small heartbeat packets

  Serial.printf("Received %d bytes from BMS\n", length);

  float voltage = 0;
  float current = 0;
  int soc = -1;

  for (int i = 0; i < length - 4; i++) {
    if (pData[i] == 0x83) { // Voltage Tag
      voltage = ((pData[i+1] << 8) | pData[i+2]) * 0.01;
    }
    if (pData[i] == 0x84) { // Current Tag
      int16_t rawCur = (pData[i+1] << 8) | pData[i+2];
      current = rawCur * 0.01;
    }
    if (pData[i] == 0x85) { // SOC Tag
      soc = pData[i+1];
    }
  }

  if (soc != -1) {
    Serial.println("============================");
    Serial.print("VOLTAGE: "); Serial.print(voltage); Serial.println(" V");
    Serial.print("CURRENT: "); Serial.print(current); Serial.println(" A");
    Serial.print("SOC:     "); Serial.print(soc);     Serial.println(" %");
    Serial.println("============================");
  }
}

bool connectToBMS() {
  Serial.println("Connecting...");
  BLEClient* pClient = NimBLEDevice::createClient();

  // Set connection parameters for stability
  pClient->setConnectionParams(12, 12, 0, 60);

  if (!pClient->connect(bmsAddress)) return false;

  BLERemoteService* pService = pClient->getService(serviceUUID);
  if (!pService) return false;

  pRemoteCharacteristic = pService->getCharacteristic(charUUID);
  if (!pRemoteCharacteristic) return false;

  // Crucial: Subscribe to notifications before writing
  if (pRemoteCharacteristic->canNotify()) {
    if (!pRemoteCharacteristic->subscribe(true, notifyCallback)) {
      return false;
    }
  }

  connected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  
  // Enable security - some JK BMS models require this for the Notify to activate
  NimBLEDevice::init("");
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max power for C3 antenna
  
  Serial.println("JK BMS Monitor Initialized");
}

void loop() {
  if (!connected) {
    if (connectToBMS()) {
      Serial.println("Connected and Subscribed!");
    } else {
      delay(5000);
      return;
    }
  }

  // Send request every 3 seconds
  if (pRemoteCharacteristic->writeValue(requestCmd, 20, false)) {
    Serial.println("Request sent...");
  } else {
    Serial.println("Request failed, reconnecting...");
    connected = false;
  }
  
  delay(3000);
}