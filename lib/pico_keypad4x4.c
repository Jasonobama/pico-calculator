/*
|| @file pico_keypad4x4.c
|| @version 0.2
|| @author Oswaldo Hernandez, Owen Jauregui
|| @contact oswahdez00@gmail.com
||
|| @description
|| | This library provides a simple interface for using a 4x4 matrix
|| | keypad with the Raspberry Pi Pico.
*/

#include "pico_keypad4x4.h"

#define GPIO_INPUT false
#define GPIO_OUTPUT true

uint _columns[4];
uint _rows[4];
char _matrix_values[16];

uint all_columns_mask = 0x0;
uint column_mask[4];

/**
 * @brief Set up the keypad
 *
 * @param columns Pins connected to keypad columns
 * @param rows Pins connected to keypad rows
 * @param matrix_values values assigned to each key
 */
void pico_keypad_init(uint columns[4], uint rows[4], char matrix_values[16]) {

    for (int i = 0; i < 16; i++) {
        _matrix_values[i] = matrix_values[i];
    }

    for (int i = 0; i < 4; i++) {

        _columns[i] = columns[i];
        _rows[i] = rows[i];

        gpio_init(_columns[i]);
        gpio_init(_rows[i]);

        gpio_set_dir(_columns[i], GPIO_INPUT);
        gpio_set_dir(_rows[i], GPIO_OUTPUT);

        gpio_put(_rows[i], 1);

        all_columns_mask = all_columns_mask + (1 << _columns[i]);
        column_mask[i] = 1 << _columns[i];
    }
}

/**
 * @brief Scan and get the pressed key.
 *
 * This routine returns the first key found to be pressed
 * during the scan.
 */
char pico_keypad_get_key(void) {
    static char last_key = 0;
    int row;
    uint32_t cols;

    cols = gpio_get_all();
    cols = cols & all_columns_mask;
    
    if (cols == 0x0) {
        last_key = 0;
        return 0;
    }

    for (int j = 0; j < 4; j++) {
        gpio_put(_rows[j], 0);
    }

    for (row = 0; row < 4; row++) {

        gpio_put(_rows[row], 1);

        busy_wait_us(10000);

        cols = gpio_get_all();
        gpio_put(_rows[row], 0);
        cols = cols & all_columns_mask;
        if (cols != 0x0) {
            break;
        }   
    }

    for (int i = 0; i < 4; i++) {
        gpio_put(_rows[i], 1);
    }

    char current_key = 0;

    if (cols == column_mask[0]) {
        current_key = (char)_matrix_values[row * 4 + 0];
    }
    else if (cols == column_mask[1]) {
        current_key = (char)_matrix_values[row * 4 + 1];
    }
    else if (cols == column_mask[2]) {
        current_key = (char)_matrix_values[row * 4 + 2];
    }
    else if (cols == column_mask[3]) {
        current_key = (char)_matrix_values[row * 4 + 3];
    }
    else {
        last_key = 0;
        return 0;
    }

    if (current_key == last_key) {
        return 0;
    }
    last_key = current_key;
    return current_key;
}

/**
 * @brief Setup keypad to work with IRQ.
 *
 * Make keypad pins work as processor interruption
 *
 * @param enable Enable or disable interruption pins
 * @param callback Function of what will happen if the key is pressed
 */
void pico_keypad_irq_enable(bool enable, gpio_irq_callback_t callback) {
    for (int i = 0; i < 4; i++) {
        gpio_set_irq_enabled_with_callback(_columns[i], 0x8, enable, callback);
    }
}
