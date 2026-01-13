#pragma once

#include <driver/gpio.h>

// ##########################
// ## Hardware definitions ##
// ##########################

#define APP_CONFIG_PIN_BUZZER GPIO_NUM_12

#define APP_CONFIG_RFID_UART_NUM UART_NUM_1
#define APP_CONFIG_PIN_RFID_UART_RX GPIO_NUM_32
#define APP_CONFIG_PIN_RFID_UART_TX GPIO_NUM_39
#define APP_CONFIG_PIN_RFID_ENABLE GPIO_NUM_33
#define APP_CONFIG_RFID_UART_RX_BAUD 2400

#define APP_CONFIG_I2C_NUM I2C_NUM_0
#define APP_CONFIG_PIN_I2C_SDA GPIO_NUM_21
#define APP_CONFIG_PIN_I2C_SCL GPIO_NUM_22

#define APP_CONFIG_I2C_ADDR_TOF400C_VL53L0X 0x52
#define APP_CONFIG_I2C_ADDR_MPU6050 0x68
#define APP_CONFIG_I2C_ADDR_HMC5883 0x1E

// ########################
// ## Global definitions ##
// ########################

#define APP_CONFIG_RFID_UART_RX_BUF_SIZE 256
