/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
const int readPin = 32; // Use GPIO number. See ESP32 board pinouts
const int LED = 2; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


class HBridgeMaderController {
  private:
    int pinArray[6] = {     // The pins used as output
      13,                 // motor 1 pin A
      12,                 // motor 1 pin B
      14,                  // motor 2 pin A
      27,                 // motor 2 pin B
      16, // EN1
      17  // EN2
    };
    int direction[5][6] = {
      { 1, 0, 1, 0, 1, 1 },     // Forward =0
      { 0, 1, 0, 1, 0, 0 },     // Backwords =1
      { 0, 0, 0, 0, 0, 0 },     // Standby =2
      { 1, 0, 0, 0, 0, 0 },     // turn left =3
      { 0, 0, 1, 0, 0, 0 }      // Turn right = 4
    };
    int pinCount = 6;       // Pins uses in array
    int speed = 100;    // for the pwm
    
    
  public:
    HBridgeMaderController() {
      for (int count = 0; count <= pinCount - 2; count++)
      {
          pinMode(this->pinArray[count], OUTPUT);
      }
      
      ledcSetup(0, 5000, 8);
      // attach the channel to the GPIO to be controlled
      ledcAttachPin(this->pinArray[4], 0);

      ledcSetup(1, 5000, 8);
      // attach the channel to the GPIO to be controlled
      ledcAttachPin(this->pinArray[5], 1);
    }

    void drive(int x) { // Driveing the pins off of the input of x.
        Serial.println("inside drive");
        for (int i = 0; i < this->pinCount; i++)
        {
            if (this->direction[x][i] == 1)
            {
                Serial.print("writing");
                Serial.print(x);
                Serial.print(i);
                Serial.println(this->direction[x][i]);
                if (i > 3) { // pwm
                  Serial.print("writing pwm");
                  Serial.println(this->speed);
                  ledcWrite(0, this->speed);
                  ledcWrite(1, this->speed);
                }
                else { // digital
                  Serial.print("writing gpio");
                  digitalWrite(this->pinArray[i], HIGH);  
                }
            }
            else
            {
                Serial.print("writing");
                Serial.print(x);
                Serial.print(i);
                Serial.println(this->direction[x][i]);
                digitalWrite(this->pinArray[i], LOW);
            }
        }
        Serial.println("outside drive");
    }

    
};
  
   
HBridgeMaderController motor_controller;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

int serial_state = 53;
int state = 2;

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {

        if(rxValue.length() > 6){

            serial_state = rxValue[5]; // state is askii!
            
            switch(serial_state){
              case 49: state = 0; break;
              case 50: state = 1; break;
              case 51: state = 3; break;
              case 52: state = 4; break;
              case 53: state = 2; break;
              default:
                state = 2;
            }
            
            motor_controller.drive(state);
        }
      }
    }
};



void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  // Create the BLE Device
  BLEDevice::init("ESP32 UART Test"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {

    
    // Fabricate some arbitrary junk for now...
    txValue = analogRead(readPin) / 3.456; // This could be an actual sensor reading!

    // Let's convert the value to a char array:
    char txString[8]; // make sure this is big enuffz
    dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer
    
//    pCharacteristic->setValue(&txValue, 1); // To send the integer value
//    pCharacteristic->setValue("Hello!"); // Sending a test message
    pCharacteristic->setValue(txString);
    
    pCharacteristic->notify(); // Send the value to the app!
    /*
    Serial.print("*** Sent Value: ");
    Serial.print(txString);
    Serial.println(" ***");
    */

    // You can add the rxValue checks down here instead
    // if you set "rxValue" as a global var at the top!
    // Note you will have to delete "std::string" declaration
    // of "rxValue" in the callback function.
//    if (rxValue.find("A") != -1) { 
//      Serial.println("Turning ON!");
//      digitalWrite(LED, HIGH);
//    }
//    else if (rxValue.find("B") != -1) {
//      Serial.println("Turning OFF!");
//      digitalWrite(LED, LOW);
//    }
  }
  delay(1000);
}
