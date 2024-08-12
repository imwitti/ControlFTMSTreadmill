#include "BLEDevice.h"

// UUIDs for treadmill services and characteristics
static BLEUUID serviceUUID((uint16_t)0x1826);
static BLEUUID speedCharUUID((uint16_t)0x2ACD);
static BLEUUID controlCharUUID((uint16_t)0x2AD9); // Control Point Characteristic UUID

// Global variables for BLE connection
static BLEAddress *pServerAddress;
static BLERemoteCharacteristic* pSpeedCharacteristic;
static BLERemoteCharacteristic* pControlCharacteristic;
static boolean doConnect = false;
static boolean connected = false;
static uint16_t inst_speed = 0;
static uint32_t total_distance = 0;

// Callback function to handle notifications from treadmill
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  // Extract speed and distance from the received data
  inst_speed = (pData[3] << 8) | pData[2];
  total_distance = ((long)pData[6] << 16) + ((long)pData[5] << 8) + pData[4];

  // Display the extracted data
  Serial.print("Instantaneous Speed: ");
  Serial.println(inst_speed);
  Serial.print("Total Distance: ");
  Serial.println(total_distance);
}

// Function to connect to the treadmill server
bool connectToServer(BLEAddress pAddress) {
  Serial.print("Connecting to: ");
  Serial.println(pAddress.toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(pAddress)) {
    Serial.println("Connection failed!");
    return false;
  }
  Serial.println("Connected!");

  // Get the treadmill service
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found!");
    pClient->disconnect();
    return false;
  }

  // Get the speed characteristic
  pSpeedCharacteristic = pRemoteService->getCharacteristic(speedCharUUID);
  if (pSpeedCharacteristic == nullptr) {
    Serial.println("Speed characteristic not found!");
    pClient->disconnect();
    return false;
  }
  pSpeedCharacteristic->registerForNotify(notifyCallback);

  // Get the control point characteristic
  pControlCharacteristic = pRemoteService->getCharacteristic(controlCharUUID);
  if (pControlCharacteristic == nullptr) {
    Serial.println("Control characteristic not found!");
    pClient->disconnect();
    return false;
  }

  return true;
}

// Advertised device callback class
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.print("Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      advertisedDevice.getScan()->stop();
      doConnect = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Client...");

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

// Function to request control of the treadmill
bool requestControl() {
  if (connected && pControlCharacteristic) {
    uint8_t controlRequest[] = {0x00}; // Op Code 0x00 to request control
    pControlCharacteristic->writeValue(controlRequest, sizeof(controlRequest), true);

    Serial.println("Requested control of the treadmill.");
    delay(100); // Small delay to ensure the request is processed
    return true;
  }
  return false;
}

// Function to set the incline of the treadmill
void setTreadmillIncline(int16_t incline) {
  if (connected && pControlCharacteristic) {
    uint8_t command[3];
    command[0] = 0x03; // Op Code 0x03 to set target inclination
    command[1] = incline & 0xFF; // LSB of inclination
    command[2] = (incline >> 8) & 0xFF; // MSB of inclination

    pControlCharacteristic->writeValue(command, sizeof(command), true);
    Serial.print("Incline set to: ");
    Serial.println(incline * 0.1); // Convert to percent
  }
}

void loop() {
  if (doConnect) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("Connected to treadmill!");
      connected = true;
    } else {
      Serial.println("Failed to connect.");
    }
    doConnect = false;
  }

  if (connected) {
    // Request control of the treadmill
    if (requestControl()) {
      // Example: set incline to 50 (5.0%)
      setTreadmillIncline(50);
    }
  }

  delay(1000);
}
