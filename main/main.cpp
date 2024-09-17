/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
//#include "driver/gpio.h"
#include "driver/twai.h"

#include "CanboxRaiseHandler.h"
#include "Car.h"


#define BUF_SIZE (1024)

#define TXD_PIN (GPIO_NUM_21)
#define RXD_PIN (GPIO_NUM_20)

#define TX_GPIO_NUM (GPIO_NUM_5)
#define RX_GPIO_NUM (GPIO_NUM_6)

static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_LISTEN_ONLY);

static const char *TAG = "CAN Decoder";


void Init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART Driver installed");

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI Driver installed");

    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI Driver started");
    
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    //ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    CanboxRaiseHandler* handler = (CanboxRaiseHandler*)arg;
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        //sendData(TX_TASK_TAG, "Hello world");
        //ESP_LOGI(TX_TASK_TAG, "send tx bytes");
        handler->SendCarState();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE + 1);

    CanboxRaiseHandler* handler = (CanboxRaiseHandler*)arg;

    while (1) {
        //ESP_LOGI(RX_TASK_TAG, "Start read from rx");
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        //ESP_LOGI(RX_TASK_TAG, "Read %d bytes", rxBytes);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            for (size_t i = 0; i < rxBytes; i++)
            {
                handler->CmdProcess(data[i]);
            }
            
        }
    }
    free(data);
}

static void canRecieveDataTask(void *arg)
{
    Car* car = (Car*)arg;
    while(1)
    {
        twai_message_t rx_msg;
        esp_err_t resCan = twai_receive(&rx_msg, 5000);
        if (resCan != ESP_OK)
        {
            ESP_LOGE(TAG, "(%s)", esp_err_to_name(resCan));
            continue;
        }

        car->ProcessCanMessage(&rx_msg);
    }
}

extern "C" void app_main(void)
{
    Init();

    CanboxRaiseHandler handler = {};
    Car car = {};
    car.InitCar();
    handler.SetCar(&car);

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, (void*)&handler, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, (void*)&handler, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(canRecieveDataTask, "canRecieveDataTask", 4096, (void*)&car, configMAX_PRIORITIES - 3, NULL);
}
