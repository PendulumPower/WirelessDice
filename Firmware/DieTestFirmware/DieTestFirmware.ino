#include <Wire.h>
#include <bluefruit.h>
#define BMA400_ADDR   0x14
#define INT_PIN       2
#define SCL_PIN       3
#define SDA_PIN       4
#define REG_ACC_X_LSB   0x04
#define REG_ACC_CONFIG1 0x19
#define REG_INT_CONFIG0 0x1F
#define REG_INT1_MAP    0x21
#define REG_WKUP_CFG    0x29
void writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(BMA400_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
void readAccel(int16_t &x, int16_t &y, int16_t &z) {
  Wire.beginTransmission(BMA400_ADDR);
  Wire.write(REG_ACC_X_LSB);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)BMA400_ADDR, (uint8_t)6);
  if (Wire.available() >= 6) {
    uint8_t xl = Wire.read(), xh = Wire.read();
    uint8_t yl = Wire.read(), yh = Wire.read();
    uint8_t zl = Wire.read(), zh = Wire.read();
    x = (int16_t)((xh << 8) | xl); if (x > 2047) x -= 4096;
    y = (int16_t)((yh << 8) | yl); if (y > 2047) y -= 4096;
    z = (int16_t)((zh << 8) | zl); if (z > 2047) z -= 4096;
  }
}
int getRolledFace() { //This is probably all going to be wrong since I'm not exactly sure how it's oriented yet.
  int16_t x, y, z;
  readAccel(x, y, z);
  int16_t ax = abs(x), ay = abs(y), az = abs(z);
  if (az >= ax && az >= ay) {
    return (z > 0) ? 1 : 6;
  } else if (ax >= ay && ax >= az) {
    return (x > 0) ? 2 : 5;
  } else {
    return (y > 0) ? 3 : 4;
  }
}
void broadcastFace(int face) {
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  uint8_t payload[5] = {
    0x4C, 0x00,
    0x06,
    (uint8_t)face,
    0x00
  };
  Bluefruit.Advertising.addManufacturerData(payload, sizeof(payload));
  Bluefruit.Advertising.setInterval(160, 160);
  Bluefruit.Advertising.start(0);
  delay(500);
  Bluefruit.Advertising.stop();
}
void setupBMA400Wakeup() {
  writeRegister(REG_ACC_CONFIG1, 0x01);
  writeRegister(REG_INT_CONFIG0, 0x04);
  writeRegister(REG_INT1_MAP,    0x04);
  writeRegister(REG_WKUP_CFG,    0x28);
}
void enterDeepSleep() {
  nrf_gpio_cfg_sense_input(INT_PIN, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
  NRF_POWER->SYSTEMOFF = 1;
  while (1);
}
void setup() {
  Wire.setPins(SDA_PIN, SCL_PIN);
  Wire.begin();
  if (NRF_POWER->RESETREAS & POWER_RESETREAS_OFF_Msk) {
    NRF_POWER->RESETREAS = POWER_RESETREAS_OFF_Msk;
    delay(400);
    int face = getRolledFace();
    broadcastFace(face);
  }
  setupBMA400Wakeup();
  enterDeepSleep();
}
void loop() {
}