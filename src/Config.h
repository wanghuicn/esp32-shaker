#pragma once
#include <Arduino.h>

// --- 硬件引脚定义 ---
#define PIN_SENSOR 32       // 震动开关
#define PIN_BUTTON 13       // 交互按键
#define PIN_BUZZER 14       // 无源蜂鸣器
#define PIN_STATUS_LED 23   // 独立状态指示灯 (WS2812)
#define PIN_BATT 34         // 电池电压检测 (ADC)
#define PIN_MIC 35          // 麦克风运放输出 (ADC)

// --- 摇摇棒微调参数 ---
const int PIXEL_DELAY_US = 200;    // 单列像素停留时间 (微秒)
const int TRIGGER_OFFSET_MS = 30;  // 触发偏移时间 (毫秒)
const int DEBOUNCE_MS = 100;       // 挥动消抖死区 (毫秒)
const unsigned long IDLE_TIMEOUT_MS = 1000; // 闲置多久进入待机动画 (毫秒)

// --- 系统模式枚举 ---
enum SystemMode {
    MODE_CALIBRATE = 0, // 0: 校准
    MODE_VOLTAGE,       // 1: 电压
    MODE_TIME,          // 2: 时钟
    MODE_PICTURE,       // 3: 图片
    MODE_CHINESE,       // 4: 中文滚动
    MODE_VU_METER,      // 5: 音频拾音条
    MODE_STANDBY,       // 6: 待机黑屏
    MODE_COUNT          // 模式总数
};

// --- 跨文件共享的全局变量 (extern 声明) ---
extern volatile SystemMode currentMode;
extern volatile int currentImgIdx;     
extern volatile uint8_t globalBright; 
extern volatile unsigned long lastTriggerTime;
extern volatile int currentVolumeLevel; 
extern float batteryVoltage;
extern String timeString;