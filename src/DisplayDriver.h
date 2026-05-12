#pragma once
#include <NeoPixelBus.h>
#include "Config.h"
#include "AsciiFont.h"

#define NUM_STRIPS 5
#define LEDS_PER_STRIP 8

// 5 路 I2S DMA 主显示屏
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip1(LEDS_PER_STRIP, 19);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip2(LEDS_PER_STRIP, 18);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip3(LEDS_PER_STRIP, 27);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip4(LEDS_PER_STRIP, 26);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip5(LEDS_PER_STRIP, 25);

// 1 路 RMT 独立状态灯
NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> statusLed(1, PIN_STATUS_LED);

inline void initDisplay() {
    strip1.Begin(); strip2.Begin(); strip3.Begin(); strip4.Begin(); strip5.Begin();
    statusLed.Begin();
    statusLed.ClearTo(RgbColor(0)); statusLed.Show();
}

inline RgbColor DimColor(uint32_t hexColor, uint8_t brightness) {
    uint8_t r = (hexColor >> 16) & 0xFF;
    uint8_t g = (hexColor >> 8) & 0xFF;
    uint8_t b = hexColor & 0xFF;
    return RgbColor((r * brightness) / 255, (g * brightness) / 255, (b * brightness) / 255);
}

inline void showAllStrips() {
    strip1.Show(); strip2.Show(); strip3.Show(); strip4.Show(); strip5.Show();
}

inline void clearAllStrips() {
    strip1.ClearTo(RgbColor(0)); strip2.ClearTo(RgbColor(0)); 
    strip3.ClearTo(RgbColor(0)); strip4.ClearTo(RgbColor(0)); strip5.ClearTo(RgbColor(0));
}

inline void setPixel(int row, RgbColor color) {
    int stripIdx = row / 8;  
    int pixelIdx = row % 8;  
    switch(stripIdx) {
        case 0: strip1.SetPixelColor(pixelIdx, color); break;
        case 1: strip2.SetPixelColor(pixelIdx, color); break;
        case 2: strip3.SetPixelColor(pixelIdx, color); break;
        case 3: strip4.SetPixelColor(pixelIdx, color); break;
        case 4: strip5.SetPixelColor(pixelIdx, color); break;
    }
}

// ASCII 文本渲染器
inline void drawText(String text, RgbColor color) {
    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c < 32 || c > 126) c = '?'; 
        int charIdx = c - 32;
        int width = font_width[charIdx];
        const uint8_t* charData = font_index[charIdx];
        
        for (int col = 0; col < width; col++) {
            clearAllStrips();
            for (int row = 0; row < 40; row++) {
                if (charData[col * 5 + (row / 8)] & (1 << (row % 8))) {
                    setPixel(row, color);
                }
            }
            showAllStrips();
            delayMicroseconds(PIXEL_DELAY_US);
        }
        clearAllStrips(); showAllStrips();
        delayMicroseconds(PIXEL_DELAY_US * 3); // 字符间距
    }
}

// 待机流水灯动画
inline void drawIdleAnimation(SystemMode mode, int volumeLevel, uint8_t bright) {
    clearAllStrips();
    unsigned long t = millis();
    
    if (mode == MODE_VU_METER) {
        // 实时音量柱
        for (int row = 39; row >= 40 - volumeLevel; row--) {
            RgbColor c = (row > 25) ? DimColor(0x00FF00, bright) : 
                         (row > 10) ? DimColor(0xFFFF00, bright) : DimColor(0xFF0000, bright);
            setPixel(row, c);
        }
    } else {
        // 跑马流星灯
        int pos = (t / 40) % 78; 
        if (pos > 39) pos = 78 - pos; 
        
        RgbColor c(0);
        switch(mode) {
            case MODE_CALIBRATE: c = DimColor(0x880088, bright); break;
            case MODE_VOLTAGE:   c = DimColor(0x888800, bright); break;
            case MODE_TIME:      c = DimColor(0x008888, bright); break;
            case MODE_PICTURE:   c = DimColor(0x008800, bright); break;
            case MODE_CHINESE:   c = DimColor(0xFF8800, bright); break;
            default: break;
        }
        setPixel(pos, c);
        if (pos < 39) setPixel(pos + 1, DimColor(c.R<<16|c.G<<8|c.B, bright/4)); 
        if (pos > 0)  setPixel(pos - 1, DimColor(c.R<<16|c.G<<8|c.B, bright/4));
    }
    showAllStrips();
}