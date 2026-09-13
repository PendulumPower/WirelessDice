#include <SPI.h>
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

TFT_eSPI tft = TFT_eSPI(); 

// Track current state to prevent display flickering
uint8_t lastState = 255;
uint8_t lastValue = 255;

// Function to handle drawing to the LCD
void updateDisplay(uint8_t state, uint8_t dieType, uint8_t value) {
  if (state == lastState && value == lastValue) return; // Prevent unnecessary redraws
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 50);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);

  if (state == 0) { // State 0: Dice is currently in motion
    tft.setTextColor(TFT_YELLOW);
    tft.printf("Rolling D%d...", dieType);
  } else if (state == 1) { // State 1: Dice has settled
    tft.setTextColor(TFT_GREEN);
    tft.printf("D%d Rolled:", dieType);
    
    tft.setCursor(10, 100);
    tft.setTextSize(6);
    tft.printf("%d!", value);
  }
  
  lastState = state;
  lastValue = value;
}

// BLE Callback to process detected devices
class DiceBLECallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Check if the detected device is our die
    if (advertisedDevice.getName() == "BLE_DICE") {
      
      // Assuming the die broadcasts a 3-byte payload in its manufacturer data:
      // Byte 0: State (0 = Rolling, 1 = Settled)
      // Byte 1: Die Type (e.g., 6 for D6, 20 for D20)
      // Byte 2: Face Value (1-6, 1-20, etc.)
      std::string mfgData = advertisedDevice.getManufacturerData();
      
      if (mfgData.length() >= 3) {
        uint8_t state = mfgData[0];
        uint8_t dieType = mfgData[1];
        uint8_t value = mfgData[2];
        
        updateDisplay(state, dieType, value);
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // Initialize LCD
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_BLACK);
  
  // Turn on Backlight (Pin D6)
  pinMode(D6, OUTPUT);
  digitalWrite(D6, HIGH);

  tft.setCursor(10, 50);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);
  tft.println("Waiting for dice...");

  // Initialize BLE Scanner
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new DiceBLECallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); 
}

void loop() {
  // Continuously scan for 2 seconds, non-blocking logic can be added if needed
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->start(2, false);
  pBLEScan->clearResults(); 
  delay(10);
}