#include <Arduino.h>
#include <OneButton.h>
#include "Config.h"
#include "DisplayDriver.h"
#include "NetworkManager.h"
#include "ImageGallery.h"
#include "ChineseText.h"

// ==========================================
// 全局变量实体化
// ==========================================
volatile SystemMode currentMode = MODE_PICTURE; 
volatile int currentImgIdx = 0;     
volatile uint8_t globalBright = 60; 
volatile unsigned long lastTriggerTime = 0;
volatile int currentVolumeLevel = 0; 

float batteryVoltage = 0.0;
String timeString = "WAIT...";

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
        case MODE_CHINESE:   c = RgbColor(50, 20, 0); break; // 橙色
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
// 硬件中断：时间窗口屏蔽法 (消除重影与连击)
// ==========================================
void IRAM_ATTR sensorISR() {
    unsigned long now = millis();
    
    // 350ms 是“屏蔽窗口期”（包含单次挥动 + 整个挥回来的时间）
    // 如果你挥得特别快，可以把 350 稍微改小 (比如 250)
    // 如果挥得比较慢，或者依然有回挥的重影，把 350 改大 (比如 400)
    if (now - lastTriggerTime > 250) { 
        lastTriggerTime = now;
        
        if (currentMode != MODE_STANDBY) {
            BaseType_t xWoken = pdFALSE;
            vTaskNotifyGiveFromISR(DisplayTaskHandle, &xWoken);
            if (xWoken) portYIELD_FROM_ISR(); 
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

        case MODE_CHINESE: {
            // 【中文模式】：独享 200 列超宽视窗！
            int TEXT_VIEW_WIDTH = 200; 
            
            // 核心黑科技：因为列数翻倍了，如果延时不减半，挥动一次的时间会太长导致图像显示不全。
            // 我们给文字模式单独设置一个缩短的延时（比如原来的二分之一）
            int textDelay = PIXEL_DELAY_US / 2; 

            int offset = (millis() / 20) % CHINESE_TEXT_WIDTH;
            
            // 循环 200 次，画出更宽的画面
            for (int col = 0; col < TEXT_VIEW_WIDTH; col++) {
                int data_col = (offset + col) % CHINESE_TEXT_WIDTH;
                clearAllStrips();
                for (int row = 0; row < 40; row++) {
                    if (chinese_text_data[data_col][row / 8] & (1 << (row % 8))) {
                        setPixel(row, DimColor(0xFF8800, globalBright));
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
    pinMode(PIN_SENSOR, INPUT_PULLUP);

    xTaskCreatePinnedToCore(LogicTask, "LogicTask", 8192, NULL, 1, &LogicTaskHandle, 0);
    xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 8192, NULL, 2, &DisplayTaskHandle, 1);

    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), sensorISR, FALLING);
}

void loop() { vTaskDelete(NULL); }