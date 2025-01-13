#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "mux.pio.h"
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "tusb.h"


#define MUX_PIN_GROUPS 2
#define MUX_GROUP_SIZE 4

#define MUX_GROUP_0_IN_START 2
#define MUX_GROUP_0_OUT_START 6

#define MUX_GROUP_1_IN_START 10
#define MUX_GROUP_1_OUT_START 14


#define INPUT_BUFFER_SIZE 101 // 100 chars + null terminator
#define TIMEOUT_MS 50         // 50 ms timeout

struct pin_map {
    uint8_t in_pin;
    uint8_t out_pin;
    PIO pio;
    uint sm;
    uint offset;
};

struct plain_pin_map {
    uint8_t in_pin;
    uint8_t out_pin;
};

struct arr_pin_map {
    plain_pin_map pin_map[MUX_PIN_GROUPS * MUX_GROUP_SIZE];
};


pin_map mux_map[MUX_PIN_GROUPS * MUX_GROUP_SIZE];

void reconfigure_mux(arr_pin_map new_setting){
    // Turn off all state machines
    pio_set_sm_mask_enabled(pio0, 0b1111, false);
    pio_set_sm_mask_enabled(pio1, 0b1111, false);
    // Reconfigure the mux_map
    for(uint8_t i = 0; i < MUX_PIN_GROUPS * MUX_GROUP_SIZE; i++) {
        mux_map[i].in_pin = new_setting.pin_map[i].in_pin;
        mux_map[i].out_pin = new_setting.pin_map[i].out_pin;
        mux_program_init(mux_map[i].pio, mux_map[i].sm, mux_map[i].offset, 1.0, mux_map[i].in_pin, mux_map[i].out_pin);
    }
    // Start all state machines with a mask at once and keep their clocks in sync
    pio_enable_sm_mask_in_sync(pio0, 0b1111);
    pio_enable_sm_mask_in_sync(pio1, 0b1111);
}

void print_pin_map(){
    printf("pin_map:[\n");
    for(uint8_t i = 0; i < MUX_PIN_GROUPS * MUX_GROUP_SIZE; i++) {
        printf("    (%d, %d),\n", mux_map[i].in_pin, mux_map[i].out_pin);
    }
    printf("]\n");
}

void parse_and_reconfigure(char *input) {
    arr_pin_map new_setting;
    int pin_count = 0;
    bool used_pins[31] = {false}; // Array to track used pins (index represents pin number)

    // Parse the input
    char *start = strchr(input, '[');
    char *end = strchr(input, ']');
    if (!start || !end || start >= end) {
        printf("Error: Invalid input format\n");
        return;
    }

    // Extract the content within the brackets
    char *content = start + 1;
    *end = '\0';

    // Tokenize the string to extract pin pairs
    char *token = strtok(content, "(),");
    while (token) {
        if (pin_count >= MUX_PIN_GROUPS * MUX_GROUP_SIZE * 2) {
            printf("Error: Too many pin mappings\n");
            return;
        }

        int pin = atoi(token);
        if (pin < 0 || pin > 30) {
            printf("Error: Pin numbers must be between 0 and 30\n");
            return;
        }

        if (pin_count % 2 == 0) {
            // Input pin
            new_setting.pin_map[pin_count / 2].in_pin = pin;
        } else {
            // Output pin
            new_setting.pin_map[pin_count / 2].out_pin = pin;
        }
        pin_count++;
        token = strtok(NULL, "(),");
    }

    // Check if the correct number of pins were parsed
    if (pin_count != MUX_PIN_GROUPS * MUX_GROUP_SIZE * 2) {
        printf("Error: Incorrect number of pin mappings\n");
        return;
    }

    // Validate the pin mappings
    for (int i = 0; i < MUX_PIN_GROUPS * MUX_GROUP_SIZE; i++) {
        uint8_t in_pin = new_setting.pin_map[i].in_pin;
        uint8_t out_pin = new_setting.pin_map[i].out_pin;

        // Check if input and output pins are the same
        if (in_pin == out_pin) {
            printf("Error: Input and output pins cannot be the same (%d, %d)\n", in_pin, out_pin);
            return;
        }

        // Check for duplicate pins
        if (used_pins[in_pin]) {
            printf("Error: Pin %d is assigned multiple times\n", in_pin);
            return;
        }
        if (used_pins[out_pin]) {
            printf("Error: Pin %d is assigned multiple times\n", out_pin);
            return;
        }

        // Mark pins as used
        used_pins[in_pin] = true;
        used_pins[out_pin] = true;
    }

    // Apply the new configuration
    reconfigure_mux(new_setting);
    printf("MUX reconfigured successfully\n");
    print_pin_map();
}


void core2() {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    while (true) {
        gpio_put(25, 0);
        sleep_ms(100);
        gpio_put(25, 1);
        sleep_ms(100);
    }
}

int main() {
    // Enable UART so we can print status output
    stdio_init_all();

    vreg_set_voltage(VREG_VOLTAGE_MAX);
    set_sys_clock_khz(400000, true);

    multicore_launch_core1(core2);

    // populate the mux_map
    for(uint8_t group = 0; group < MUX_PIN_GROUPS; group++) {
        // since the code for all the PIOs is the same we use the same offset for all of them in a group (PIO instance)
        uint offset = pio_add_program(group == 0 ? pio0 : pio1, &mux_program);
        for(uint8_t i = 0; i < MUX_GROUP_SIZE; i++) {
            mux_map[group * MUX_GROUP_SIZE + i].in_pin = group == 0 ? MUX_GROUP_0_IN_START + i : MUX_GROUP_1_IN_START + i;
            mux_map[group * MUX_GROUP_SIZE + i].out_pin = group == 0 ? MUX_GROUP_0_OUT_START + i : MUX_GROUP_1_OUT_START + i;
            mux_map[group * MUX_GROUP_SIZE + i].pio = group == 0 ? pio0 : pio1;
            mux_map[group * MUX_GROUP_SIZE + i].sm = i;
            mux_map[group * MUX_GROUP_SIZE + i].offset = offset;
            mux_program_init(mux_map[group * MUX_GROUP_SIZE + i].pio, mux_map[group * MUX_GROUP_SIZE + i].sm, mux_map[group * MUX_GROUP_SIZE + i].offset, 1.0, mux_map[group * MUX_GROUP_SIZE + i].in_pin, mux_map[group * MUX_GROUP_SIZE + i].out_pin);
            // pio_sm_set_enabled(mux_map[group * MUX_GROUP_SIZE + i].pio, mux_map[group * MUX_GROUP_SIZE + i].sm, true);
        }
    }
    // Start all state machines with a mask at once and keep their clocks in sync
    pio_enable_sm_mask_in_sync(pio0, 0b1111);
    pio_enable_sm_mask_in_sync(pio1, 0b1111);

    // Wait for USB CDC to connect
    while (!tud_cdc_connected()) {
        tight_loop_contents();
    }

    sleep_ms(100);

    char input_buffer[INPUT_BUFFER_SIZE];
    int buffer_index = 0;
    absolute_time_t start_time;

    printf("Ready to accept MUX configuration\n");

    while (true) {
        if (tud_cdc_available()) {
            char c = tud_cdc_read_char();

            // Start timeout timer on first character
            if (buffer_index == 0) {
                start_time = get_absolute_time();
            }

            // Add character to the buffer
            if (buffer_index < INPUT_BUFFER_SIZE - 1) {
                input_buffer[buffer_index++] = c;
                input_buffer[buffer_index] = '\0'; // Null-terminate
            }

            // Check for end of input
            if (c == '\n') {
                parse_and_reconfigure(input_buffer);
                buffer_index = 0;
                continue;
            }
        }

        // Check for timeout
        if (buffer_index > 0 && absolute_time_diff_us(start_time, get_absolute_time()) > TIMEOUT_MS * 1000) {
            printf("Error: Input timeout\n");
            buffer_index = 0; // Reset buffer
        }
    }

    return 0;
}
