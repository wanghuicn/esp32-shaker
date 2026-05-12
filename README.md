***

# 🚀 ESP32 多功能智能摇摇棒 (Smart POV Wand)

本项目是一个基于 ESP32 的高性能多功能视觉暂留（POV）显示设备。
系统采用了 **FreeRTOS 双核架构** 与 **I2S DMA 硬件并发技术**，实现了极限帧率的图像渲染，并集成了 ADC 电池监测、WiFi NTP 网络时钟、独立状态指示灯以及多级按键交互。

## 🏗️ 系统架构总览 (System Architecture)

为了保证摇摇棒在高速挥动时画面绝对不撕裂、不卡顿，系统在 ESP32 上进行了严格的**物理双核隔离**：

*   **Core 1 (显卡核心 - 极高优先级):** 独占 I2S DMA 硬件资源，休眠等待。一旦传感器触发，瞬间唤醒并全速推流刷屏，不受任何其他逻辑干扰。
*   **Core 0 (管家核心 - 正常优先级):** 负责在后台“悠闲”地处理 WiFi 通信、按键消抖（OneButton）、ADC 电池电压计算以及蜂鸣器发声。

---

## 📂 核心文件清单与 API 手册 (File Breakdown)

整个工程由 5 个解耦的头文件和 1 个主程序组成：

### 1. ⚙️ `Config.h` (全局枢纽配置)
这是整个系统的“控制面板”。所有的硬件引脚分配、物理微调参数、全局状态变量都在这里声明。其他任何文件想要互相通信，都要通过引入 `Config.h` 来实现。

**关键变量说明：**
*   `PIXEL_DELAY_US`: 控制图像的宽窄（胖瘦）。
*   `currentMode`: 当前系统所处的模式（校准、电压、时间、图片、待机）。
*   `globalBright`: 全局灯珠亮度 (0-255)。
*   `batteryVoltage` / `timeString`: 存放后台计算好的电压和时间数据，供显示核心随时取用。

### 2. 🔠 `AsciiFont.h` & 🖼️ `ImageGallery.h` (静态数据资产)
这两个文件纯粹是**数据仓库**，里面只存放数组，没有逻辑代码。
*   `ImageGallery.h`: 存放由 Python 脚本生成的彩色图片三维数组。
*   `AsciiFont.h`: 存放由 Python 脚本生成的 40px 高度的自适应宽度点阵字库。

### 3. 🖥️ `DisplayDriver.h` (底层显示引擎)
这是系统的“显卡驱动”。它封装了所有复杂的 `NeoPixelBus` 硬件底层逻辑，对上层提供傻瓜式的绘图接口。

**核心调用函数 (API):**
*   `initDisplay()`: 初始化所有灯带（在 setup 中调用一次）。
*   `clearAllStrips()`: 瞬间黑屏。
*   `showAllStrips()`: 瞬间将内存数据推送到 5 路物理灯带（DMA 发射）。
*   `setPixel(int row, RgbColor color)`: **最常用的画笔函数**。你不需要管它在物理上是第几根灯带，直接传入 0-39 的行号和颜色，它会自动帮你映射好。
*   `drawText(String text, RgbColor color)`: 文本渲染引擎。传入一个字符串，它会自动去 `AsciiFont.h` 里查字典，并在空中把它画出来。

### 4. 🌐 `NetworkManager.h` (网络与时间服务)
负责处理所有需要联网的耗时任务。
*   `initNetworkAndTime()`: 阻塞式连接 WiFi 并请求阿里云 NTP 服务器对时。
*   `updateTime()`: 提取 ESP32 内部的 RTC 实时时钟，并格式化为 `HH:MM:SS` 存入 `timeString` 变量中供显示引擎使用。

### 5. 🧠 `main.cpp` (中枢神经系统)
这是程序的总入口。它不处理具体的硬件点灯逻辑，只负责**任务调度（RTOS）**和**状态机流转**。

**核心模块说明：**
*   **按键状态机:** 绑定了 `handleClick` (单击切换图片)、`handleLongPress` (长按切换模式)。
*   **ADC 滤波:** `readBatteryVoltage()` 中包含了一个低通滤波算法，防止电压跳变。
*   **渲染分发器 (`drawFrame`):** 这是一个巨大的 `switch` 语句。当挥动触发时，它会看一眼当前的 `currentMode`，然后决定调用哪种绘图逻辑。

---

## 🛠️ 进阶开发指南：如何添加一个新的模式？

得益于模块化的架构，现在你想为摇摇棒添加一个新功能（比如：显示当前的室内温度），步骤极其简单，**只需三步**，绝对不会破坏原有代码：

**第 1 步：在 `Config.h` 中注册新模式**
在 `enum SystemMode` 枚举中，加入你的新模式名字，例如：
```cpp
enum SystemMode {
    MODE_CALIBRATE = 0,
    MODE_VOLTAGE,
    MODE_TIME,
    MODE_PICTURE,
    MODE_TEMPERATURE, // <--- 新加的模式
    MODE_STANDBY,
    MODE_COUNT
};
```

**第 2 步：在 `main.cpp` 中分配一个指示灯颜色**
找到 `updateStatusLED()` 函数，为新模式分配一个状态灯颜色：
```cpp
case MODE_TEMPERATURE: c = RgbColor(0, 0, 50); break; // 设为蓝色
```

**第 3 步：在 `main.cpp` 的渲染器中编写画面**
找到 `drawFrame()` 函数，在 `switch (currentMode)` 里面加上你的渲染逻辑：
```cpp
case MODE_TEMPERATURE: {
    // 假设你接了一个温度传感器读到了 25度
    drawText("Temp: 25 C", DimColor(0x0000FF, globalBright));
    break;
}
```
**搞定！** 就是这么简单优雅。

---

> **给开发者的寄语：**
> 嵌入式系统的魅力在于让死板的硅基芯片拥有交互的灵魂。这个项目涉及了硬件中断、DMA、I2S、RTOS调度、状态机和图形算法。能完全啃下这套代码，你已经具备了主导开发中小型物联网终端产品的能力。继续探索吧！🚀