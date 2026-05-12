#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// ==========================================
// MPU6050 寄存器地址
// ==========================================
#define MPU6050_ADDR_0       0x68  // AD0 = LOW (默认)
#define MPU6050_ADDR_1       0x69  // AD0 = HIGH
#define MPU6050_WHO_AM_I     0x75
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

// ==========================================
// MPU6050 管理类
// ==========================================
class MPU6050Manager {
private:
    bool m_initialized;
    uint8_t m_deviceAddr;           // 实际检测到的设备地址
    int16_t m_accelX, m_accelY, m_accelZ;
    float m_ax, m_ay, m_az;       // 当前加速度 (g)
    float m_ax_prev, m_ay_prev, m_az_prev; // 上一次加速度 (g)
    unsigned long m_lastShakeTime;

    // 读取 MPU6050 寄存器
    bool readRegister(uint8_t reg, uint8_t *data, size_t len) {
        Wire.beginTransmission(m_deviceAddr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom(m_deviceAddr, len) != len) return false;
        for (size_t i = 0; i < len; i++) {
            data[i] = Wire.read();
        }
        return true;
    }

    // 写 MPU6050 寄存器
    bool writeRegister(uint8_t reg, uint8_t value) {
        Wire.beginTransmission(m_deviceAddr);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission(true) == 0;
    }

    // 尝试在指定地址初始化 MPU6050
    bool tryInit(uint8_t addr) {
        m_deviceAddr = addr;

        // 检查 WHO_AM_I
        uint8_t whoami = 0;
        if (!readRegister(MPU6050_WHO_AM_I, &whoami, 1)) {
            return false;
        }

        // WHO_AM_I 寄存器值应为 0x68 (MPU6050 的固定 ID，与 AD0 引脚无关)
        if (whoami != 0x68 && whoami != 0x69) {
            Serial.printf("[MPU6050] 地址 0x%02X: WHO_AM_I=0x%02X 不匹配\n", addr, whoami);
            return false;
        }

        // 唤醒 MPU6050 (清除 SLEEP 位)
        if (!writeRegister(MPU6050_PWR_MGMT_1, 0x00)) {
            Serial.printf("[MPU6050] 地址 0x%02X: 唤醒失败\n", addr);
            return false;
        }

        delay(10);

        // 读取一次初始值作为基准
        updateReadings();
        m_ax_prev = m_ax;
        m_ay_prev = m_ay;
        m_az_prev = m_az;

        Serial.printf("[MPU6050] 在地址 0x%02X 初始化成功！使用 MPU6050 检测挥动\n", addr);
        return true;
    }

public:
    MPU6050Manager() : m_initialized(false), m_deviceAddr(0), m_lastShakeTime(0) {
        m_ax = m_ay = m_az = 0.0f;
        m_ax_prev = m_ay_prev = m_az_prev = 0.0f;
    }

    // 初始化 MPU6050 (自动扫描 0x68 和 0x69)
    bool begin() {
        Wire.begin(PIN_MPU_SDA, PIN_MPU_SCL);
        Wire.setClock(400000); // 400kHz 快速模式

        delay(50); // 等待 MPU6050 上电稳定

        // 先尝试地址 0x68 (AD0 = LOW)
        Serial.println("[MPU6050] 尝试地址 0x68...");
        if (tryInit(MPU6050_ADDR_0)) {
            m_initialized = true;
            return true;
        }

        // 再尝试地址 0x69 (AD0 = HIGH)
        Serial.println("[MPU6050] 尝试地址 0x69...");
        if (tryInit(MPU6050_ADDR_1)) {
            m_initialized = true;
            return true;
        }

        // 两个地址都失败
        Serial.println("[MPU6050] 未检测到设备 (地址 0x68 和 0x69 均无响应)");
        m_initialized = false;
        return false;
    }

    // 更新加速度读数
    void updateReadings() {
        if (!m_initialized) return;

        uint8_t buf[6];
        if (!readRegister(MPU6050_ACCEL_XOUT_H, buf, 6)) return;

        m_accelX = (int16_t)(buf[0] << 8 | buf[1]);
        m_accelY = (int16_t)(buf[2] << 8 | buf[3]);
        m_accelZ = (int16_t)(buf[4] << 8 | buf[5]);

        // 转换为 g (默认 ±2g 量程: 16384 LSB/g)
        m_ax = m_accelX / 16384.0f;
        m_ay = m_accelY / 16384.0f;
        m_az = m_accelZ / 16384.0f;
    }

    // 检测是否发生了挥动
    // 通过比较当前合加速度与静止时合加速度 (1g) 的差异来判断
    bool detectShake() {
        if (!m_initialized) return false;

        unsigned long now = millis();
        if (now - m_lastShakeTime < MPU_SHAKE_COOLDOWN_MS) return false;

        // 读取新值
        updateReadings();

        // 计算合加速度 (矢量长度) = sqrt(ax² + ay² + az²)
        // 静止时合加速度约为 1g (重力)
        float magnitude = sqrt(m_ax * m_ax + m_ay * m_ay + m_az * m_az);

        // 计算与 1g 的偏差绝对值
        float delta = fabs(magnitude - 1.0f);

        // 如果偏差超过阈值，视为挥动
        if (delta > MPU_SHAKE_THRESHOLD) {
            m_lastShakeTime = now;
            // 同时更新上一次值用于调试
            m_ax_prev = m_ax;
            m_ay_prev = m_ay;
            m_az_prev = m_az;
            return true;
        }

        // 更新上一次值
        m_ax_prev = m_ax;
        m_ay_prev = m_ay;
        m_az_prev = m_az;

        return false;
    }

    // 获取当前加速度 (调试用)
    float getAccelX() const { return m_ax; }
    float getAccelY() const { return m_ay; }
    float getAccelZ() const { return m_az; }

    // 获取合加速度 (调试用)
    float getMagnitude() const { return sqrt(m_ax * m_ax + m_ay * m_ay + m_az * m_az); }

    // 获取检测到的设备地址
    uint8_t getDeviceAddress() const { return m_deviceAddr; }

    // 是否已初始化
    bool isInitialized() const { return m_initialized; }
};

// 全局 MPU6050 管理器实例
extern MPU6050Manager mpu6050;
