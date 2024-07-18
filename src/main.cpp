#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Nordic nRF
BLEUUID UARTServiceUUID = BLEUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID UARTRxCharacteristicUUID = BLEUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID UARTTxCharacteristicUUID = BLEUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

BLEServer* server;
BLEService* uartService;
BLECharacteristic* uartRxCharacteristic;
BLECharacteristic* uartTxCharacteristic;


bool deviceConnected = false;
bool oldDeviceConnected = false;

int messageLength = 0;
uint8_t messageBuffer[1000];


class Callbacks: public BLEServerCallbacks, public BLECharacteristicCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }

    void onWrite(BLECharacteristic *characteristic) {
      if(characteristic->getUUID().equals(UARTRxCharacteristicUUID)){
         Serial1.write(characteristic->getData(), characteristic->getLength());
         Serial1.flush();
      }
    }
};
Callbacks callbacks;

void setup() {
    // Create the BLE Device
   //BLEDevice::init("Little Focer V4");
   //BLEDevice::init("SOLO V2");
   //BLEDevice::init("ESP32 Wireless");
   //BLEDevice::init("MotoSurf ESC");
   //BLEDevice::init("Black Diamond");
   BLEDevice::init("Big Bang");

   // Create the BLE Server
   server = BLEDevice::createServer();
   server->setCallbacks(&callbacks);
   
   // Create the uart BLE Service
   uartService = server->createService(UARTServiceUUID);
   uartRxCharacteristic = uartService->createCharacteristic(UARTRxCharacteristicUUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
   uartRxCharacteristic->setCallbacks(&callbacks);
   uartTxCharacteristic = uartService->createCharacteristic(UARTTxCharacteristicUUID, BLECharacteristic::PROPERTY_NOTIFY);
   uartTxCharacteristic->addDescriptor(new BLE2902());
   uartService->start();

   // Start advertising
   server->getAdvertising()->addServiceUUID(UARTServiceUUID);
   server->getAdvertising()->start();

   // Set up serial connection to VESC
   Serial1.begin(115200, SERIAL_8N1, 20, 21, false);
}
void loop() {
   // disconnecting
   if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      server->startAdvertising(); // restart advertising
      oldDeviceConnected = deviceConnected;
   }
   // connecting
   if (deviceConnected && !oldDeviceConnected) {
      // do stuff here on connecting
      oldDeviceConnected = deviceConnected;
   }
   
   delay(1);
   messageLength = Serial1.available();
   if (messageLength > 0) {
      Serial1.readBytes(messageBuffer, messageLength);
      uartTxCharacteristic->setValue(messageBuffer, messageLength);
      uartTxCharacteristic->notify();
   }
}