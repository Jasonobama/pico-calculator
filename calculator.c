/**
 * @file calculator.c
 * @brief Raspberry Pi Pico 计算器程序
 * @details 支持加减乘除运算，使用 4x4 键盘输入，LCD1602 显示
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "lib/pico_keypad4x4.h"

// ==================== LCD1602 配置 ====================
#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_BAUDRATE 100000
#define LCD_I2C_ADDR 0x27      // 常见地址，如果不行尝试 0x3F

// LCD 指令定义
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETDDRAMADDR 0x80

#define LCD_ENTRYLEFT 0x02
#define LCD_DISPLAYON 0x04
#define LCD_2LINE 0x08
#define LCD_5x8DOTS 0x00

#define LCD_ENABLE_BIT 0x04
#define LCD_BACKLIGHT_BIT 0x08

// ==================== 计算器状态定义 ====================
typedef enum {
    STATE_INPUT_FIRST,
    STATE_INPUT_SECOND,
    STATE_RESULT
} CalcState;

// 全局变量
static uint8_t lcd_backlight = LCD_BACKLIGHT_BIT;
static CalcState calc_state = STATE_INPUT_FIRST;
static char current_input[32] = "";
static char first_num[32] = "";
static char second_num[32] = "";
static char operator_char = '\0';
static char display_buffer[17] = "";

// ==================== LCD1602 底层函数 ====================
/**
 * @brief I2C 发送数据
 */
static void lcd_i2c_write_byte(uint8_t data) {
    i2c_write_blocking(I2C_PORT, LCD_I2C_ADDR, &data, 1, false);
}

/**
 * @brief 向 LCD 发送 4 位数据
 * @param data 4 位数据（低 4 位有效）
 * @param mode 0=命令，1=数据
 */
static void lcd_send_nibble(uint8_t data, uint8_t mode) {
    uint8_t value = data | mode | lcd_backlight;
    lcd_i2c_write_byte(value);
    lcd_i2c_write_byte(value | LCD_ENABLE_BIT);
    busy_wait_us(2);
    lcd_i2c_write_byte(value & ~LCD_ENABLE_BIT);
    busy_wait_us(40);
}

/**
 * @brief 向 LCD 发送一个字节（8 位数据，分两次发送高 4 位和低 4 位）
 * @param data 要发送的数据
 * @param mode 0=命令，1=数据
 */
static void lcd_send_byte(uint8_t data, uint8_t mode) {
    lcd_send_nibble(data & 0xF0, mode);
    lcd_send_nibble((data << 4) & 0xF0, mode);
}

/**
 * @brief LCD 初始化
 */
static void lcd_init(void) {
    // 初始化 I2C
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    busy_wait_ms(50);
    
    // 初始化 LCD 为 4 位模式
    lcd_send_nibble(0x30, 0);
    busy_wait_ms(5);
    lcd_send_nibble(0x30, 0);
    busy_wait_us(150);
    lcd_send_nibble(0x30, 0);
    busy_wait_ms(2);
    lcd_send_nibble(0x20, 0);  // 切换到 4 位模式
    busy_wait_ms(2);
    
    // 功能设置：2 行，5x8 点阵
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, 0);
    // 显示开关：显示开启，光标关闭
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, 0);
    // 输入模式设置
    lcd_send_byte(LCD_ENTRYMODESET | LCD_ENTRYLEFT, 0);
    // 清屏
    lcd_send_byte(LCD_CLEARDISPLAY, 0);
    busy_wait_ms(2);
}

/**
 * @brief LCD 清屏
 */
static void lcd_clear(void) {
    lcd_send_byte(LCD_CLEARDISPLAY, 0);
    busy_wait_ms(2);
}

/**
 * @brief 设置光标位置
 * @param row 行号 (0 或 1)
 * @param col 列号 (0-15)
 */
static void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0 ? 0x00 : 0x40) + col;
    lcd_send_byte(LCD_SETDDRAMADDR | address, 0);
}

/**
 * @brief 在 LCD 上打印字符串
 * @param str 要打印的字符串
 */
static void lcd_print(const char *str) {
    while (*str) {
        lcd_send_byte(*str++, 1);
    }
}

/**
 * @brief 在指定位置显示字符串
 * @param row 行号
 * @param col 列号
 * @param str 字符串
 */
static void lcd_display_string(uint8_t row, uint8_t col, const char *str) {
    lcd_set_cursor(row, col);
    lcd_print(str);
}

/**
 * @brief 更新显示内容
 * @param input 当前输入的数字
 * @param op 运算符
 * @param is_result 是否显示结果模式
 */
static void update_display(const char *input, char op, bool is_result) {
    char line1[17] = "";
    char line2[17] = "";
    
    if (is_result) {
        snprintf(line1, 17, "Result:");
        snprintf(line2, 17, "%s", input);
    } else if (op != '\0') {
        // 有运算符时，第一行显示第一个数+运算符，第二行显示第二个数
        snprintf(line1, 17, "%s %c", first_num[0] ? first_num : "0", op);
        snprintf(line2, 17, "%s", input);
    } else {
        // 无运算符时，第一行显示算式，第二行显示输入
        snprintf(line1, 17, "Calculator:");
        snprintf(line2, 17, "%s", input);
    }
    
    lcd_clear();
    lcd_display_string(0, 0, line1);
    lcd_display_string(1, 0, line2);
}

// ==================== 计算函数 ====================
/**
 * @brief 执行计算
 * @param a 第一个操作数
 * @param b 第二个操作数
 * @param op 运算符
 * @param result 结果缓冲区
 * @return 是否计算成功
 */
static bool calculate(double a, double b, char op, char *result) {
    double res;
    switch (op) {
        case '+':
            res = a + b;
            break;
        case '-':
            res = a - b;
            break;
        case '*':
            res = a * b;
            break;
        case '/':
            if (b == 0) {
                strcpy(result, "Error: Div0");
                return false;
            }
            res = a / b;
            break;
        default:
            return false;
    }
    
    // 格式化输出
    if (res == (int)res) {
        snprintf(result, 32, "%d", (int)res);
    } else {
        snprintf(result, 32, "%.6f", res);
        // 去掉末尾多余的 0
        char *p = result + strlen(result) - 1;
        while (p > result && *p == '0') p--;
        if (*p == '.') p--;
        *(p + 1) = '\0';
    }
    return true;
}

// ==================== 键盘处理函数 ====================
/**
 * @brief 处理按键输入
 * @param key 按下的按键字符
 */
static void process_key(char key) {
    // 删除键 (0 键)
    if (key == '0') {
        if (calc_state == STATE_RESULT) {
            // 结果状态下按删除，清空所有
            calc_state = STATE_INPUT_FIRST;
            strcpy(current_input, "");
            first_num[0] = '\0';
            second_num[0] = '\0';
            operator_char = '\0';
            update_display("", '\0', false);
        } else {
            // 删除最后一个字符
            int len = strlen(current_input);
            if (len > 0) {
                current_input[len - 1] = '\0';
                if (operator_char == '\0') {
                    strcpy(first_num, current_input);
                    update_display(current_input, operator_char, false);
                } else {
                    strcpy(second_num, current_input);
                    update_display(current_input, operator_char, false);
                }
            }
        }
        return;
    }
    
    // 数字处理 (1-9, '*' 作为 0)
    if ((key >= '1' && key <= '9') || key == '*') {
        // '*' 键作为数字 0
        char digit = (key == '*') ? '0' : key;
        
        if (calc_state == STATE_RESULT) {
            // 如果处于结果状态，开始新的计算
            calc_state = STATE_INPUT_FIRST;
            strcpy(current_input, "");
            first_num[0] = '\0';
            second_num[0] = '\0';
            operator_char = '\0';
        }
        
        // 限制输入长度
        if (strlen(current_input) < 15) {
            int len = strlen(current_input);
            current_input[len] = digit;
            current_input[len + 1] = '\0';
            
            // 更新相应操作数
            if (operator_char == '\0') {
                strcpy(first_num, current_input);
                update_display(current_input, operator_char, false);
            } else {
                strcpy(second_num, current_input);
                update_display(current_input, operator_char, false);
            }
        }
        return;
    }
    
    // 运算符处理
    if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
        if (calc_state == STATE_RESULT) {
            // 如果处于结果状态，用结果作为第一个操作数开始新运算
            calc_state = STATE_INPUT_SECOND;
            strcpy(first_num, current_input);
            strcpy(current_input, "");
            second_num[0] = '\0';
        } else if (operator_char != '\0' && strlen(second_num) > 0) {
            // 已有第二个操作数，先计算结果，再用结果继续运算
            char result[32];
            double a = atof(first_num);
            double b = atof(second_num);
            if (calculate(a, b, operator_char, result)) {
                strcpy(first_num, result);
                strcpy(current_input, "");
                second_num[0] = '\0';
                calc_state = STATE_INPUT_SECOND;
            } else {
                update_display("Error", '\0', true);
                calc_state = STATE_RESULT;
                strcpy(current_input, "Error");
                return;
            }
        } else if (operator_char != '\0') {
            // 只有运算符，没有第二个数，替换运算符
            calc_state = STATE_INPUT_SECOND;
        } else {
            calc_state = STATE_INPUT_SECOND;
        }
        
        // 设置运算符
        switch (key) {
            case 'A': operator_char = '+'; break;
            case 'B': operator_char = '-'; break;
            case 'C': operator_char = '*'; break;
            case 'D': operator_char = '/'; break;
        }
        
        if (strlen(first_num) == 0) {
            strcpy(first_num, "0");
        }
        strcpy(current_input, "");
        update_display(current_input, operator_char, false);
        return;
    }
    
    // 等于号 (# 键) - 用户要求左下角 # 作为等于号
    if (key == '#') {
        if (operator_char != '\0' && strlen(second_num) > 0) {
            char result[32];
            double a = atof(first_num);
            double b = atof(second_num);
            if (calculate(a, b, operator_char, result)) {
                strcpy(current_input, result);
                strcpy(first_num, result);
                second_num[0] = '\0';
                operator_char = '\0';
                calc_state = STATE_RESULT;
                update_display(result, '\0', true);
            } else {
                update_display("Error", '\0', true);
                strcpy(current_input, "Error");
                calc_state = STATE_RESULT;
            }
        } else if (strlen(first_num) > 0 && calc_state != STATE_RESULT) {
            // 没有运算符，只是显示结果
            strcpy(current_input, first_num);
            calc_state = STATE_RESULT;
            update_display(first_num, '\0', true);
        }
        return;
    }
    
}

// ==================== 主函数 ====================
int main() {
    // 初始化标准输入输出和 USB 串口
    stdio_init_all();
    
    // 初始化 LCD
    lcd_init();
    
    // 定义 4x4 键盘的列和行 GPIO 引脚
    uint columns[4] = {18, 19, 20, 21};
    uint rows[4] = {10, 11, 12, 13};
    
    // 定义键盘字符映射
    // 第1行: 1, 2, 3, A(加号)
    // 第2行: 4, 5, 6, B(减号)
    // 第3行: 7, 8, 9, C(乘号)
    // 第4行: 0(删除), *(0), #(等于), D(除号)
    char matrix[16] = {
        '1', '2', '3', 'A',      // 第1行: A = 加法
        '4', '5', '6', 'B',      // 第2行: B = 减法
        '7', '8', '9', 'C',      // 第3行: C = 乘法
        '0', '*', '#', 'D'       // 第4行: 0=删除, *=0, #=等于, D=除法
    };
    
    // 初始化键盘
    pico_keypad_init(columns, rows, matrix);
    
    // 显示启动界面
    lcd_clear();
    lcd_display_string(0, 0, "Pico Calculator");
    lcd_display_string(1, 0, "Ready...");
    busy_wait_ms(2000);
    
    // 清空计算器状态
    strcpy(current_input, "");
    first_num[0] = '\0';
    second_num[0] = '\0';
    operator_char = '\0';
    calc_state = STATE_INPUT_FIRST;
    update_display("", '\0', false);
    
    // 主循环
    char key;
    while (true) {
        key = pico_keypad_get_key();
        if (key != '\0') {
            // 通过 USB 串口输出按键信息（用于调试）
            printf("Key pressed: %c\n", key);
            process_key(key);
        }
        busy_wait_us(100000);  // 100ms 延迟，避免 CPU 占用过高
    }
    
    return 0;
}