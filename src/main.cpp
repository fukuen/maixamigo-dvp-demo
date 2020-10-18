#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Maixduino_OV7740.h>
#include <Wire.h>

#define AXP192_ADDR 0x34
#ifdef M5STICKV
#define PIN_SDA 29
#define PIN_SCL 28
#endif

#define AXP173_ADDR 0x34
#ifdef MAIXAMIGO
#define PIN_SDA 27
#define PIN_SCL 24
#endif
#ifdef MAIXCUBE
#define PIN_SDA SDA
#define PIN_SCL SCL
#endif

TFT_eSPI lcd;
//OV7740 supports YUV only
//Maixduino_OV7740 camera(FRAMESIZE_QVGA, PIXFORMAT_YUV422);
Maixduino_OV7740 camera(480, 320, PIXFORMAT_YUV422);

bool axp192_init() {
    Serial.printf("AXP192 init.\n");
    sysctl_set_power_mode(SYSCTL_POWER_BANK3,SYSCTL_POWER_V33);

    Wire.begin((uint8_t) PIN_SDA, (uint8_t) PIN_SCL, 400000);
    Wire.beginTransmission(AXP192_ADDR);
    int err = Wire.endTransmission();
    if (err) {
        Serial.printf("Power management ic not found.\n");
        return false;
    }
    Serial.printf("AXP192 found.\n");

    // Clear the interrupts
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x46);
    Wire.write(0xFF);
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x23);
    Wire.write(0x08); //K210_VCore(DCDC2) set to 0.9V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x33);
    Wire.write(0xC1); //190mA Charging Current
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x36);
    Wire.write(0x6C); //4s shutdown
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x91);
    Wire.write(0xF0); //LCD Backlight: GPIO0 3.3V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x90);
    Wire.write(0x02); //GPIO LDO mode
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x28);
    Wire.write(0xF0); //VDD2.8V net: LDO2 3.3V,  VDD 1.5V net: LDO3 1.8V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x27);
    Wire.write(0x2C); //VDD1.8V net:  DC-DC3 1.8V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x12);
    Wire.write(0xFF); //open all power and EXTEN
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x23);
    Wire.write(0x08); //VDD 0.9v net: DC-DC2 0.9V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x31);
    Wire.write(0x03); //Cutoff voltage 3.2V
    Wire.endTransmission();
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(0x39);
    Wire.write(0xFC); //Turnoff Temp Protect (Sensor not exist!)
    Wire.endTransmission();

    fpioa_set_function(23, (fpioa_function_t)(FUNC_GPIOHS0 + 26));
    gpiohs_set_drive_mode(26, GPIO_DM_OUTPUT);
    gpiohs_set_pin(26, GPIO_PV_HIGH); //Disable VBUS As Input, BAT->5V Boost->VBUS->Charing Cycle

    msleep(20);
    return true;
}

void axp173_init() {
    Wire.begin((uint8_t) PIN_SDA, (uint8_t) PIN_SCL, 400000);
    Wire.beginTransmission(AXP173_ADDR);
    int err = Wire.endTransmission();
    if (err) {
        Serial.printf("Power management ic not found.\n");
        return;
    }
    Serial.printf("AXP173 found.\n");
#ifdef MAIXAMIGO
    //LDO4 - 0.8V (default 0x48 1.8V)
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x27);
    Wire.write(0x20);
    Wire.endTransmission();
    //LDO2/3 - LDO2 1.8V / LDO3 3.0V
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x28);
    Wire.write(0x0C);
    Wire.endTransmission();
#else
    // Clear the interrupts
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x46);
    Wire.write(0xFF);
    Wire.endTransmission();
    // set target voltage and current of battery(axp173 datasheet PG.)
    // charge current (default)780mA -> 190mA
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x33);
    Wire.write(0xC1);
    Wire.endTransmission();
    // REG 10H: EXTEN & DC-DC2 control
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x10);
    Wire.endTransmission();
    Wire.requestFrom(AXP173_ADDR, 1, 1);
    int reg = Wire.read();
    Wire.beginTransmission(AXP173_ADDR);
    Wire.write(0x10);
    Wire.write(reg & 0xFC);
    Wire.endTransmission();
#endif
}

void setup() {
    Serial.begin(115200);
#ifdef M5STICKV
    axp192_init();
#endif
#ifdef MAIXAMIGO
    axp173_init();
#endif

    /* LCD init */
    lcd.begin();
    lcd.setRotation(1);

    /* DVP init */
    Serial.printf("DVP init\n");
    if (!camera.begin2(1)) {
        Serial.printf("camera init fail\n");
        while (true) {}
    } else {
        Serial.printf("camera init success\n");
    }
    Serial.printf("id: %d ¥n", camera.id());
    camera.setRotaion(2);
    camera.run(true);
}

void loop() {
    uint8_t *img = camera.snapshot();
    if (img == nullptr || img == 0) {
        Serial.printf("snap fail\n");
        return;
    }

    lcd.pushImage(0, 0, camera.width(), camera.height(), (uint16_t*)img);
}
