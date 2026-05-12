#pragma once
#include <WiFi.h>
#include <time.h>
#include "Config.h"

const char* ssid = "Redmi K50 Pro";        // 修改为你的WiFi
const char* password = "96886596"; // 修改为你的密码

inline void initNetworkAndTime() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500); Serial.print("."); attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        configTime(8 * 3600, 0, "ntp.aliyun.com", "time.pool.aliyun.com");
    } else {
        Serial.println("\nWiFi Failed.");
    }
}

inline void updateTime() {
    struct tm timeinfo;
    // 【核心修复】：加上 10 的参数，最多只等 10ms，绝不阻塞按键任务！
    if (getLocalTime(&timeinfo, 10)) {
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        timeString = String(timeStr);
    } else {
        timeString = "NO SYNC";
    }
}