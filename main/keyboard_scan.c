/* 
Keyboard driver
(c) Roger Spooner 2023

Scan the rows and columns of the keyboard looking for connections.
The keys are in a grid with diodes.
Make sure we know the GPIOs for the rows and columns.
Init:
- Set all rows and columns to be inputs.
- Pull down resistors on the rows.
- Initialise "previous" list of pressed keys to empty.
Loop:
- Copy the previous list of pressed keys.
- Initialise list of pressed keys to be empty.
- Scan through the columns one at a time.
   - Make the column output HIGH.
   - Scan through the rows one at a time.
   -    If the row is HIGH, then the key is pressed.
   -    Add this key to the array of pressed keys.
- If the list of pressed keys is different from the previous list, then
  log changes. However the USB protocol doesn't need changes, just currently
  pressed keys.
  We should debounce this too, e.g. measure the time at which the key last changed
    */

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "keyboard_scan.h"

#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 4
#define KEY_DEBOUNCE_DELAY 3 /* measured in scan iterations */
static gpio_num_t keyboard_row_io[KEYBOARD_ROWS] = { GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_14 };
static gpio_num_t keyboard_col_io[KEYBOARD_COLS] = { GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_32, GPIO_NUM_33 };
static int key_last_changed_time[KEYBOARD_COLS][KEYBOARD_ROWS];
static uint8_t keys_down[2][KEYBOARD_COLS][KEYBOARD_ROWS];
static int scan_index = 0;
static int keys_down_index = 0;

static const char *TAG = "keyboard_scan";

esp_err_t keyboard_init()
{
    ESP_LOGI(TAG, "keyboard_init");
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        gpio_reset_pin(keyboard_row_io[row]);
        gpio_set_direction(keyboard_row_io[row], GPIO_MODE_INPUT);
        gpio_set_pull_mode(keyboard_row_io[row], GPIO_PULLDOWN_ONLY);
    }
    for (int col = 0; col < KEYBOARD_COLS; col++) {
        gpio_reset_pin(keyboard_col_io[col]);
        gpio_set_direction(keyboard_col_io[col], GPIO_MODE_INPUT);
        gpio_set_pull_mode(keyboard_col_io[col], GPIO_PULLDOWN_ONLY);
    }
     for (int col = 0; col < KEYBOARD_COLS; col++) {
        for (int row = 0; row < KEYBOARD_ROWS; row++) {
            key_last_changed_time[col][row] = 0;
        }
    }
    for (int k = 0; k < 2; k++)
        reset_keys_down(k);
    keys_down_index = 0;
    scan_index = 0;
    return 0;
}

void reset_keys_down(int k)
{
    for (int col = 0; col < KEYBOARD_COLS; col++) {
        for (int row = 0; row < KEYBOARD_ROWS; row++) {
            keys_down[k][col][row] = 0;
        }
    }
}

esp_err_t keyboard_scan()
{
    keys_down_index ^= 1;
    scan_index += 1;
    reset_keys_down(keys_down_index);
    int n_keys_down = 0;
    for (int col = 0; col < KEYBOARD_COLS; col++) {
        gpio_set_direction(keyboard_col_io[col], GPIO_MODE_OUTPUT);
        gpio_set_level(keyboard_col_io[col], 1);
        for (int row = 0; row < KEYBOARD_ROWS; row++) {
            if (gpio_get_level(keyboard_row_io[row]) == 1 
                && ( key_last_changed_time[col][row] < scan_index - KEY_DEBOUNCE_DELAY
                     || key_last_changed_time[col][row] == 0)) {
                n_keys_down++;
                keys_down[keys_down_index][col][row] = 1;
                key_last_changed_time[col][row] = scan_index;
                ESP_LOGI(TAG, "key down at %d,%d", col, row);
            }
        }
        gpio_set_direction(keyboard_col_io[col], GPIO_MODE_INPUT);
        gpio_set_pull_mode(keyboard_col_io[col], GPIO_PULLDOWN_ONLY);
    }
    return 0;
};