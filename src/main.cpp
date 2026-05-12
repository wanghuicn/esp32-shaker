#include <Arduino.h>
#include <OneButton.h>
#include "Config.h"
#include "DisplayDriver.h"
#include "NetworkManager.h"
#include "ImageGallery.h"
#include "ChineseText.h"
#include "MPU6050Manager.h"

// ==========================================
// 全局变量实体化
// ==========================================
volatile SystemMode currentMode = MODE_PICTURE; 
volatile int currentImgIdx = 0;     
volatile uint8_t globalBright =200; 
volatile unsigned long lastTriggerTime = 0;
volatile int currentVolumeLevel = 0; 

float batteryVoltage = 0.0;
String timeString = "WAIT...";
String serialInputString = "";       // 串口输入字符串
bool serialStringUpdated = false;    // 串口字符串更新标志

bool useMPU6050 = false;           // 传感器选择标志
MPU6050Manager mpu6050;            // MPU6050 管理器实例

TaskHandle_t DisplayTaskHandle = NULL;
TaskHandle_t LogicTaskHandle = NULL;
OneButton button(PIN_BUTTON, true, true);

// ==========================================
// 功能支持函数
// ==========================================
void beep(int freq, int duration) { tone(PIN_BUZZER, freq, duration); }

void readBatteryVoltage() {
    uint32_t pin_mV = analogReadMilliVolts(PIN_BATT);
    // 这里的 7.191 请根据你实测万用表后计算出的真实分压比替换
    float rawVoltage = (pin_mV * 7.191) / 1000.0; 
    
    if (batteryVoltage == 0.0) batteryVoltage = rawVoltage;
    else batteryVoltage = (batteryVoltage * 0.9) + (rawVoltage * 0.1);
}

void updateStatusLED() {
    RgbColor c(0);
    switch (currentMode) {
        case MODE_CALIBRATE: c = RgbColor(50, 0, 50); break;
        case MODE_VOLTAGE:   c = RgbColor(50, 50, 0); break;
        case MODE_TIME:      c = RgbColor(0, 50, 50); break;
        case MODE_PICTURE:   c = RgbColor(0, 50, 0); break;
        case MODE_CHINESE:   c = RgbColor(0, 0, 50); break;  // 蓝色
        case MODE_SERIAL:    c = RgbColor(50, 0, 50); break;  // 紫色
        case MODE_VU_METER:  c = RgbColor(0, 0, 50); break;  // 蓝色
        case MODE_STANDBY:   c = RgbColor(50, 0, 0); break;
        default: break;
    }
    statusLed.SetPixelColor(0, c);
    statusLed.Show();
}

// ==========================================
// 按键交互
// ==========================================
void handleClick() {
    if (currentMode == MODE_PICTURE) {
        currentImgIdx = (currentImgIdx + 1) % NUM_IMAGES;
        beep(2000, 50);
    } else {
        beep(500, 50);
    }
}

void handleLongPress() {
    currentMode = (SystemMode)((currentMode + 1) % MODE_COUNT);
    updateStatusLED();
    beep(4000, 200); delay(100); beep(4000, 200); 
}

// ==========================================
// 硬件中断：震动开关中断 (仅当 MPU6050 不可用时使用)
// ==========================================
void IRAM_ATTR sensorISR() {
    unsigned long now = millis();
    
    // 250ms 屏蔽窗口期
    if (now - lastTriggerTime > 250) { 
        lastTriggerTime = now;
        
        if (currentMode != MODE_STANDBY) {
            BaseType_t xWoken = pdFALSE;
            vTaskNotifyGiveFromISR(DisplayTaskHandle, &xWoken);
            if (xWoken) portYIELD_FROM_ISR(); 
        }
    }
}

// ==========================================
// MPU6050 轮询检测 (在 LogicTask 中调用)
// ==========================================
void checkMPU6050Shake() {
    if (!useMPU6050) return; // 未使用 MPU6050，跳过
    
    if (mpu6050.detectShake()) {
        unsigned long now = millis();
        lastTriggerTime = now;
        
        // 串口输出挥动检测信息 (含合加速度)
        Serial.printf("[MPU6050] 检测到挥动! AX=%.2f AY=%.2f AZ=%.2f |MAG|=%.2f\n", 
                      mpu6050.getAccelX(), mpu6050.getAccelY(), mpu6050.getAccelZ(),
                      mpu6050.getMagnitude());
        
        if (currentMode != MODE_STANDBY) {
            // 通知 DisplayTask
            BaseType_t xWoken = pdFALSE;
            vTaskNotifyGiveFromISR(DisplayTaskHandle, &xWoken);
            // 注意：这里不在 ISR 中，不需要 portYIELD
        }
    }
}

void drawFrame() {
    switch (currentMode) {
        case MODE_CALIBRATE:
            for (int col = 0; col < 100; col++) {
                clearAllStrips();
                for (int row = 0; row < 40; row++) {
                    if (row == 0 || row == 39) setPixel(row, DimColor(0xFF0000, globalBright));
                    if (col == 0 || col == 99) setPixel(row, DimColor(0x0000FF, globalBright));
                    if (col == (row * 100 / 40)) setPixel(row, DimColor(0x00FF00, globalBright));
                }
                showAllStrips(); delayMicroseconds(PIXEL_DELAY_US);
            }
            break;

        case MODE_VOLTAGE: {
            char vStr[10]; sprintf(vStr, "V:%.2fV", batteryVoltage);
            drawText(String(vStr), DimColor(0xFFFF00, globalBright));
            break;
        }

        case MODE_TIME:
            drawText(timeString, DimColor(0x00FFFF, globalBright));
            break;

        case MODE_PICTURE:
            for (int col = 0; col < 100; col++) {
                for (int row = 0; row < 40; row++) {
                    setPixel(row, DimColor(image_gallery[currentImgIdx][col][row], globalBright));
                }
                showAllStrips(); delayMicroseconds(PIXEL_DELAY_US);
            }
            break;

        case MODE_SERIAL: {
            // 串口输入显示模式：显示串口输入的英文/数字字符串
            String displayStr = serialInputString;
            if (displayStr.length() == 0) displayStr = "WAIT...";
            drawText(displayStr, DimColor(0xFF00FF, globalBright));
            break;
        }

        case MODE_CHINESE: {
            // 【中文模式】：独享 200 列超宽视窗！
            int TEXT_VIEW_WIDTH = 200; 
            
            // 核心黑科技：因为列数翻倍了，如果延时不减半，挥动一次的时间会太长导致图像显示不全。
            // 我们给文字模式单独设置一个缩短的延时（比如原来的二分之一）
            int textDelay = PIXEL_DELAY_US / 2; 

            int offset = (millis() / 15) % CHINESE_TEXT_WIDTH;
            
            // 循环 200 次，画出更宽的画面
            for (int col = 0; col < TEXT_VIEW_WIDTH; col++) {
                int data_col = (offset + col) % CHINESE_TEXT_WIDTH;
                clearAllStrips();
                for (int row = 0; row < 40; row++) {
                    if (chinese_text_data[data_col][row / 8] & (1 << (row % 8))) {
                        setPixel(row, DimColor(0x0000FF, globalBright));
                    }
                }
                showAllStrips(); 
                delayMicroseconds(textDelay); // 使用专属的快速延时
            }
            break;
        }

        case MODE_VU_METER: {
            for (int col = 0; col < 100; col++) {
                clearAllStrips();
                for (int row = 39; row >= 40 - currentVolumeLevel; row--) {
                    RgbColor c = (row > 15) ? DimColor(0x00FF00, globalBright) : DimColor(0xFF0000, globalBright);
                    setPixel(row, c);
                }
                showAllStrips(); delayMicroseconds(PIXEL_DELAY_US);
            }
            break;
        }
        case MODE_STANDBY: break;
    }
    clearAllStrips(); showAllStrips();
}

// ==========================================
// Core 1: 专职显卡任务
// ==========================================
void DisplayTask(void *pvParameters) {
    initDisplay();
    updateStatusLED();

    for (;;) {
        uint32_t waved = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        
        if (waved > 0) {
            delay(TRIGGER_OFFSET_MS);
            drawFrame(); 
        } else {
            if (millis() - lastTriggerTime > IDLE_TIMEOUT_MS) {
                if (currentMode != MODE_STANDBY) drawIdleAnimation(currentMode, currentVolumeLevel, globalBright);
                else { clearAllStrips(); showAllStrips(); }
            } else {
                clearAllStrips(); showAllStrips();
            }
        }
    }
}

// ==========================================
// Core 0: 后台管家任务
// ==========================================
void LogicTask(void *pvParameters) {
    pinMode(PIN_BUZZER, OUTPUT);
    analogSetAttenuation(ADC_0db); // 启用0dB最高精度ADC，配合 analogReadMilliVolts
    
    button.attachClick(handleClick);
    button.attachLongPressStart(handleLongPress);
    initNetworkAndTime(); 
    
    unsigned long last1sTimer = 0;

    for (;;) {
        button.tick(); 
        
        // MPU6050 挥动检测 (如果使用 MPU6050)
        checkMPU6050Shake();
        
        // 音频高频采样
        if (currentMode == MODE_VU_METER) {
            int max_v = 0, min_v = 4095;
            unsigned long start = millis();
            while(millis() - start < 10) {
                int v = analogRead(PIN_MIC);
                if(v > max_v) max_v = v;
                if(v < min_v) min_v = v;
            }
            int swing = max_v - min_v;
            if (swing < 150) swing = 0; 
            int lvl = map(swing, 150, 2500, 0, 40); 
            if (lvl > 40) lvl = 40; if (lvl < 0) lvl = 0;
            
            if (lvl >= currentVolumeLevel) currentVolumeLevel = lvl;
            else currentVolumeLevel--; 
        }

        // 串口数据读取 (在串口模式下持续读取)
        if (currentMode == MODE_SERIAL && Serial.available()) {
            String newStr = Serial.readStringUntil('\n');
            newStr.trim();
            if (newStr.length() > 0) {
                serialInputString = newStr;
                serialStringUpdated = true;
                Serial.printf("[串口] 收到: %s\n", serialInputString.c_str());
            }
        }

        // 1 秒低频任务
        if (millis() - last1sTimer > 1000) {
            last1sTimer = millis();
            readBatteryVoltage();
            updateTime();
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// ==========================================
// 初始化
// ==========================================
void setup() {
    Serial.begin(115200);
    
    // 尝试初始化 MPU6050
    Serial.println("[系统] 正在检测 MPU6050...");
    useMPU6050 = mpu6050.begin();
    
    if (!useMPU6050) {
        // MPU6050 不可用，使用震动开关
        Serial.println("[系统] MPU6050 未检测到，回退到震动开关模式");
        pinMode(PIN_SENSOR, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), sensorISR, FALLING);
    } else {
        // MPU6050 可用，震动开关引脚保持高阻态（不初始化）
        Serial.println("[系统] 使用 MPU6050 检测挥动");
    }

    xTaskCreatePinnedToCore(LogicTask, "LogicTask", 8192, NULL, 1, &LogicTaskHandle, 0);
    xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 8192, NULL, 2, &DisplayTaskHandle, 1);
}

void loop() { vTaskDelete(NULL); }
