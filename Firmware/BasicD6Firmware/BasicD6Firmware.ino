#include <Wire.h>
#include <bluefruit.h>

// Hardware Pin Definitions
#define BMA400_ADDR   0x14  // SDO tied to GND sets I2C address to 0x14
#define INT_PIN       2     // BMA400 INT1 -> nRF52 P0.02
#define SCL_PIN       3     // BMA400 SCL  -> nRF52 P0.03
#define SDA_PIN       4     // BMA400 SDA  -> nRF52 P0.04

// BMA400 Register Map
#define BMA400_REG_ACC_X_LSB    0x04
#define BMA400_REG_ACC_CONFIG1  0x19
#define BMA400_REG_INT_CONFIG0  0x1F
#define BMA400_REG_INT1_MAP     0x21
#define BMA400_REG_WKUP_INT_CFG 0x29

struct AccelData {
  int16_t x;
  int16_t y;
  int16_t z;
};

// I2C Helper Functions
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BMA400_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

AccelData readAccel() {
  AccelData data = {0, 0, 0};
  Wire.beginTransmission(BMA400_ADDR);
  Wire.write(BMA400_REG_ACC_X_LSB);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)BMA400_ADDR, (uint8_t)6);

  if (Wire.available() >= 6) {
    uint8_t x_lsb = Wire.read();
    uint8_t x_msb = Wire.read();
    uint8_t y_lsb = Wire.read();
    uint8_t y_msb = Wire.read();
    uint8_t z_lsb = Wire.read();
    uint8_t z_msb = Wire.read();

    // Reconstruct 12-bit signed values
    int16_t raw_x = (int16_t)((x_msb << 8) | x_lsb);
    if (raw_x > 2047) raw_x -= 4096;
    
    int16_t raw_y = (int16_t)((y_msb << 8) | y_lsb);
    if (raw_y > 2047) raw_y -= 4096;

    int16_t raw_z = (int16_t)((z_msb << 8) | z_lsb);
    if (raw_z > 2047) raw_z -= 4096;

    data.x = raw_x;
    data.y = raw_y;
    data.z = raw_z;
  }
  return data;
}

// Feature 1: Supercapacitor Power-On Guard
void checkBrownoutSafety() {
  delay(15); // Allow power rail to stabilize from dock inrush
  if (NRF_POWER->RESETREAS & POWER_RESETREAS_RESETPIN_Msk) {
    NRF_POWER->RESETREAS = POWER_RESETREAS_RESETPIN_Msk;
  }
}

// Feature 2: Dynamic Settle Detection
void waitForSettle() {
  AccelData prev = readAccel();
  uint8_t stable_count = 0;

  // Poll until change across consecutive reads drops below threshold
  while (stable_count < 5) {
    delay(40);
    AccelData curr = readAccel();

    int32_t deltaX = abs(curr.x - prev.x);
    int32_t deltaY = abs(curr.y - prev.y);
    int32_t deltaZ = abs(curr.z - prev.z);

    if ((deltaX + deltaY + deltaZ) < 35) {
      stable_count++;
    } else {
      stable_count = 0;
    }
    prev = curr;
  }
}

// Feature 3: Gravity Vector Face Orientation Calculation
int calculateD6Face() {
  AccelData acc = readAccel();

  int16_t absX = abs(acc.x);
  int16_t absY = abs(acc.y);
  int16_t absZ = abs(acc.z);

  // Map primary 1g acceleration axis to the upward face
  if (absZ >= absX && absZ >= absY) {
    return (acc.z > 0) ? 1 : 6;
  } else if (absX >= absY && absX >= absZ) {
    return (acc.x > 0) ? 2 : 5;
  } else {
    return (acc.y > 0) ? 3 : 4;
  }
}

// BLE Broadcast Transmission
void broadcastResult(int face) {
  Bluefruit.begin();
  Bluefruit.setTxPower(4); // Set to max transmit power (+4 dBm)
  
  uint8_t beaconData[3] = {
    0x06,            // D6 Die Type
    (uint8_t)face,   // Roll Result
    0x00
  };
  
  Bluefruit.Advertising.addManufacturerData(0x004C, beaconData, 3);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 160); // 100 ms advertising interval
  Bluefruit.Advertising.start(0);
  
  delay(500); // 500 ms broadcast window
  Bluefruit.Advertising.stop();
}

// Feature 4: Full BMA400 Low-Power Wakeup Configuration
void configureBMA400Sleep() {
  // 1. Put BMA400 into Low Power mode (0x01)
  writeRegister(BMA400_REG_ACC_CONFIG1, 0x01);

  // 2. Enable Wake-up Interrupt engine (0x04)
  writeRegister(BMA400_REG_INT_CONFIG0, 0x04);

  // 3. Map Wake-up Interrupt to physical INT1 pin (0x04)
  writeRegister(BMA400_REG_INT1_MAP, 0x04);

  // 4. Configure Wake-up threshold across X, Y, Z axes
  writeRegister(BMA400_REG_WKUP_INT_CFG, 0x28);
}

void enterSystemOff() {
  // Configure nRF52 P0.02 (INT1) to sense HIGH signal for wake-up
  nrf_gpio_cfg_sense_input(INT_PIN, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
  
  // Power down peripherals and enter deep System-OFF mode
  NRF_POWER->SYSTEMOFF = 1;
  while(1);
}

void setup() {
  checkBrownoutSafety();
  Wire.begin(SDA_PIN, SCL_PIN); // SDA=P0.04, SCL=P0.03
  
  // Verify if boot was triggered by motion wake-up from System OFF
  if (NRF_POWER->RESETREAS & POWER_RESETREAS_OFF_Msk) {
    NRF_POWER->RESETREAS = POWER_RESETREAS_OFF_Msk; // Clear reset flag
    
    waitForSettle();
    int face = calculateD6Face();
    broadcastResult(face);
  }

  configureBMA400Sleep();
  enterSystemOff();
}

void loop() {
  // Unused: nRF52 performs a cold boot on wake from System OFF
}