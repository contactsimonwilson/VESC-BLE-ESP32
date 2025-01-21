#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEUUID UARTServiceUUID = BLEUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID UARTRxCharacteristicUUID = BLEUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID UARTTxCharacteristicUUID = BLEUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

BLEServer* server;
BLEService* uartService;
BLECharacteristic* uartRxCharacteristic;
BLECharacteristic* uartTxCharacteristic;

uint8_t MTU_SIZE = 128;
uint8_t PACKET_SIZE = MTU_SIZE - 3;

bool deviceConnected = false;

int messageLength = 0;
uint8_t messageBuffer[1000];

static const char* TAG = "VESCBLE";

void setMtu(uint16_t mtu) {
   MTU_SIZE = mtu;
   PACKET_SIZE = MTU_SIZE - 3;
}

class Callbacks: public BLEServerCallbacks, public BLECharacteristicCallbacks {
    void onConnect(BLEServer* pServer) {
      uint16_t mtu = pServer->getPeerMTU(pServer->getConnId());
      setMtu(mtu);
      deviceConnected = true;
      log_i("Device connected with MTU: %d", mtu);
    }

    void onDisconnect(BLEServer* pServer) {
      log_i("Device disconnected");
      deviceConnected = false;
          // Optional: More aggressive cleanup
      pServer->getAdvertising()->stop();
      delay(100);
      pServer->getAdvertising()->start();
    }

    void onWrite(BLECharacteristic *characteristic) {
      if(characteristic->getUUID().equals(UARTRxCharacteristicUUID)){
         Serial1.write(characteristic->getData(), characteristic->getLength());
         Serial1.flush();
      }
    }

    void onNotify(BLECharacteristic *characteristic) {
      log_i("Notification sent");
    }

    void onMtuChanged(uint16_t mtu) {
      log_i("MTU Size changed: %d", mtu);
      setMtu(mtu);
    }
};

Callbacks callbacks;

void setup() {
   // Create the BLE Device
   BLEDevice::init("Little Focer V4");
   //BLEDevice::init("SOLO V2");
   //BLEDevice::init("ESP32 Wireless");
   //BLEDevice::init("MotoSurf ESC");
   //BLEDevice::init("Black Diamond");
   //BLEDevice::init("Big Bang");
   BLEDevice::setPower(ESP_PWR_LVL_P9);
   BLEDevice::setMTU(MTU_SIZE);

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
   log_i("Init complete");
}

void loop() {
   messageLength = Serial1.available();
   if (messageLength > 0) {
      messageLength = Serial1.readBytes(messageBuffer, messageLength);
      log_i("Message length: %d", messageLength);

      int remaining = messageLength;
      int packetSize = PACKET_SIZE;
      int packetIndex = 0;

      while (remaining > 0) {
         packetSize = remaining < PACKET_SIZE ? remaining : PACKET_SIZE;
         log_i("Packet size: %d", packetSize);
         packetIndex = messageLength - remaining;
         u_int8_t payload[packetSize];
         memcpy(payload, &messageBuffer[packetIndex], packetSize * sizeof(u_int8_t));
         uartTxCharacteristic->setValue(payload, packetSize);
         remaining -= packetSize;
         uartTxCharacteristic->notify();
         delay(2); // avoid ble stack congestion
      }
      memset(messageBuffer, 0, messageLength);
   }
}