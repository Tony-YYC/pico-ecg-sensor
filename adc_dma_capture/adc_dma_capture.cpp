/**
 * Modified from Raspberry Pi example for continuous ECG capture
 * Uses 12-bit ADC resolution and outputs actual voltage values
 * Samples ADC at 500Hz for 5 seconds (2500 samples) in a loop
 */

#include <pico/time.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#define CAPTURE_CHANNEL 0      // Channel 0 is ADC0/GPIO26
constexpr int16_t CAPTURE_DEPTH = 5000;     // 5 seconds of data at 500Hz
constexpr float ADC_VREF = 3.3f;         // ADC reference voltage
constexpr float ADC_CONVERSION_FACTOR = (ADC_VREF / (1 << 12));  // For 12-bit ADC
constexpr float SAMPLE_RATE = 1000.0f;        // should be at least 732Sa/s
constexpr float CLOCK_DIV = 47999.0f;

uint16_t capture_buf[CAPTURE_DEPTH];  // Using uint16_t for 12-bit values
uint dma_chan;  // Make DMA channel global so we can reuse it

void init_adc_and_dma() {
    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(26 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 12 bit reads; disable.
        false    // Don't shift down to 8 bits
    );

    // Calculate appropriate clock divider for 500Hz sampling
    adc_set_clkdiv(CLOCK_DIV);

    // Claim DMA channel once at initialization
    dma_chan = dma_claim_unused_channel(true);
}

void adc_capture_loop() {
    // Configure DMA for this capture
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);  // 16-bit transfers for 12-bit ADC
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    printf("Starting new capture at %d Hz for %d samples...\n", SAMPLE_RATE, CAPTURE_DEPTH);

    // Clear FIFO before starting
    adc_fifo_drain();
    adc_run(false);

    // Configure and start DMA
    dma_channel_configure(dma_chan, &cfg,
        capture_buf,    // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        true           // start immediately
    );

    adc_run(true);

    // Wait for capture to complete
    dma_channel_wait_for_finish_blocking(dma_chan);
    
    // Stop the ADC
    adc_run(false);
    adc_fifo_drain();

    // Print header
    printf("Time(ms),Voltage(V)\n");

    // Print samples to stdout with actual voltage values
    for (int i = 0; i < CAPTURE_DEPTH; ++i) {
        float time_ms = (float)i * (1000.0f / SAMPLE_RATE);
        float voltage = capture_buf[i] * ADC_CONVERSION_FACTOR;
        printf("%.1f,%.3f\n", time_ms, voltage);
        sleep_us(20);  // Small delay to prevent overwhelming the serial output
    }

    printf("Capture complete\n\n");
}

void cleanup_dma() {
    dma_channel_unclaim(dma_chan);
}

int main() {
    stdio_init_all();
    
    // Initialize ADC and DMA once
    init_adc_and_dma();

    printf("Starting continuous ECG capture with 12-bit resolution\n");
    printf("ADC voltage reference: %.1fV\n", ADC_VREF);
    printf("Sample rate: %dHz\n", SAMPLE_RATE);
    printf("Capture duration: %d samples (%d seconds)\n\n", 
           CAPTURE_DEPTH, CAPTURE_DEPTH/SAMPLE_RATE);

    // Capture loop
    while(1) {
        adc_capture_loop();
        sleep_ms(1000);  // Wait between captures
    }

    // This won't be reached in an infinite loop, but good practice
    cleanup_dma();
    return 0;
}