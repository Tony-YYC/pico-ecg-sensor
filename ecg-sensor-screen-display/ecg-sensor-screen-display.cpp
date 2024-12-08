/**
 * ECG Display and Heart Rate Monitor
 * Combines ADC capture with LCD display and heart rate detection
 */

#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>
#include <stdio.h>
#include <cmath>
#include "lcd_wrapper.hpp"  // 使用新的包装头文件

#define CAPTURE_CHANNEL 0
#define CAPTURE_DEPTH 2500  // 2.5 seconds of data at 1000Hz
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 135
#define ECG_AMPLITUDE 50    // Vertical scaling for ECG display
#define ECG_OFFSET 90       // Vertical center position for ECG trace
#define R_PEAK_THRESHOLD 2.3 // Voltage threshold for R peak detection (adjust as needed)

// Global variables
uint16_t capture_buf[CAPTURE_DEPTH];
uint dma_chan;
UWORD *display_buf;
uint32_t last_heartbeat_time = 0;
float heart_rate = 0.0f;

// Constants from adc_dma_capture.cpp
constexpr float ADC_VREF = 3.3f;
constexpr float ADC_CONVERSION_FACTOR = (ADC_VREF / (1 << 12));
constexpr float SAMPLE_RATE = 1000.0f;
constexpr float CLOCK_DIV = 47999.0f;

// 在全局变量区添加滤波器系数和状态
struct BandpassFilter {
    // 二阶IIR滤波器的状态变量
    float x1 = 0, x2 = 0;  // 输入历史
    float y1 = 0, y2 = 0;  // 输出历史
    
    // 滤波器系数 (0.5-35Hz @ 1000Hz采样率)
    // 使用二阶Butterworth滤波器设计
    static constexpr float b0 = 0.0675f;
    static constexpr float b1 = 0.0f;
    static constexpr float b2 = -0.0675f;
    static constexpr float a1 = -1.8650f;
    static constexpr float a2 = 0.8651f;
    
    float process(float input) {
        // 实现IIR滤波器
        float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        
        // 更新状态
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        
        return output;
    }
    
    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
    }
};

void init_adc_and_dma() {
    adc_gpio_init(26 + CAPTURE_CHANNEL);
    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(CLOCK_DIV);
    dma_chan = dma_claim_unused_channel(true);
}

void init_display() {
    if(DEV_Module_Init() != 0) {
        printf("Display init failed!\n");
        return;
    }
    
    DEV_SET_PWM(50);  // Set backlight
    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(BLACK);
    
    // Allocate display buffer
    display_buf = (UWORD *)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    if(display_buf == NULL) {
        printf("Failed to allocate display buffer\n");
        return;
    }
    
    // Initialize Paint
    Paint_NewImage((UBYTE *)display_buf, LCD_1IN14.WIDTH, LCD_1IN14.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(BLACK);
    Paint_SetRotate(ROTATE_0);
}

void calculate_heart_rate(float voltage) {
    static uint32_t r_peak_count = 0;
    static uint32_t last_peak_time = 0;
    static const uint32_t MIN_RR_INTERVAL_MS = 200; // Minimum time between peaks (300bpm max)
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Check for R peak
    if (voltage > R_PEAK_THRESHOLD && 
        (current_time - last_peak_time) > MIN_RR_INTERVAL_MS) {
        
        if (last_peak_time > 0) {
            // Calculate instantaneous heart rate
            float rr_interval = (current_time - last_peak_time) / 1000.0f; // Convert to seconds
            float instant_hr = 60.0f / rr_interval;
            
            // Update heart rate with moving average
            heart_rate = heart_rate * 0.7f + instant_hr * 0.3f;
        }
        
        last_peak_time = current_time;
        r_peak_count++;
    }
}

void display_ecg_data() {
    static float filtered_buf[CAPTURE_DEPTH];
    static const int DISPLAY_SAMPLES = DISPLAY_WIDTH;
    static float display_samples[DISPLAY_WIDTH];
    static BandpassFilter filter;  // 滤波器实例
    
    // 首先应用带通滤波器
    for(int i = 0; i < CAPTURE_DEPTH; i++) {
        float voltage = capture_buf[i] * ADC_CONVERSION_FACTOR;
        filtered_buf[i] = filter.process(voltage);
    }
    
    // 智能降采样使用局部最大最小值方法
    const int DOWNSAMPLE_FACTOR = CAPTURE_DEPTH / DISPLAY_WIDTH;
    for(int i = 0; i < DISPLAY_WIDTH; i++) {
        float min_val = 99999.0f;
        float max_val = -99999.0f;
        int start_idx = i * DOWNSAMPLE_FACTOR;
        int end_idx = start_idx + DOWNSAMPLE_FACTOR;
        
        // 在此窗口中找到局部最大和最小值
        for(int j = start_idx; j < end_idx && j < CAPTURE_DEPTH; j++) {
            min_val = min_val > filtered_buf[j] ? filtered_buf[j] : min_val;
            max_val = max_val < filtered_buf[j] ? filtered_buf[j] : max_val;
        }
        
        // 使用与均值偏差较大的值作为代表点
        float mean = (min_val + max_val) / 2.0f;
        float dist_to_min = fabs(mean - min_val);
        float dist_to_max = fabs(mean - max_val);
        
        display_samples[i] = (dist_to_max > dist_to_min) ? max_val : min_val;
    }
    
    // 清除显示缓冲区
    Paint_Clear(BLACK);
    
    // 绘制网格
    for (int x = 0; x < DISPLAY_WIDTH; x += 20) {
        Paint_DrawLine(x, 0, x, DISPLAY_HEIGHT, GRAY, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    }
    for (int y = 0; y < DISPLAY_HEIGHT; y += 20) {
        Paint_DrawLine(0, y, DISPLAY_WIDTH, y, GRAY, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    }
    
    // 绘制ECG数据
    for (int i = 1; i < DISPLAY_WIDTH; i++) {
        // 缩放电压值到显示坐标
        int y1 = ECG_OFFSET - (int)(display_samples[i-1] * ECG_AMPLITUDE);
        int y2 = ECG_OFFSET - (int)(display_samples[i] * ECG_AMPLITUDE);
        
        // 确保坐标在显示范围内
        y1 = (y1 < 0) ? 0 : (y1 >= DISPLAY_HEIGHT ? DISPLAY_HEIGHT - 1 : y1);
        y2 = (y2 < 0) ? 0 : (y2 >= DISPLAY_HEIGHT ? DISPLAY_HEIGHT - 1 : y2);
        
        Paint_DrawLine(i-1, y1, i, y2, BLUE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        
        // 使用滤波后的数据进行心率计算
        calculate_heart_rate(display_samples[i]);
    }
    
    // 显示心率
    char hr_str[32];
    snprintf(hr_str, sizeof(hr_str), "HR: %.0f BPM", heart_rate);
    Paint_DrawString_EN(5, 5, hr_str, &Font16, BLACK, GREEN);
    
    // 更新显示
    LCD_1IN14_Display(display_buf);
}

void capture_and_display() {
    // Configure DMA
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    
    // Start new capture
    adc_fifo_drain();
    adc_run(false);
    
    dma_channel_configure(dma_chan, &cfg,
        capture_buf,    // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        true           // start immediately
    );
    
    adc_run(true);
    
    // Wait for capture to complete
    dma_channel_wait_for_finish_blocking(dma_chan);
    
    // Stop ADC
    adc_run(false);
    adc_fifo_drain();
    
    // Display the captured data
    display_ecg_data();
}

int main() {
    stdio_init_all();
    
    // Initialize ADC, DMA, and Display
    init_adc_and_dma();
    init_display();
    
    printf("Starting ECG monitoring...\n");
    
    // Main loop
    while(1) {
        capture_and_display();
        sleep_ms(100);  // Small delay between captures
    }
    
    // Cleanup (never reached in infinite loop)
    free(display_buf);
    dma_channel_unclaim(dma_chan);
    DEV_Module_Exit();
    
    return 0;
}