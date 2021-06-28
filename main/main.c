/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "io_helpers.h"
#include "config_handler.h"
#include "wifi_handler.h"
#include "io_config.h"
#include "io_setup_handler.h"
#include "io_handler.h"



#define WIFI_SSID_LEN 33
#define WIFI_PASS_LEN 65
void app_main(void)
{
    printf("HTTP server example booting...\n");

    // init subsystems
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* app_netif = wifi_init_sta();
    
    // allow user to clear config
    clear_user_config_handler();

    // configure, setup and connect wifi
    char wifi_ssid[WIFI_SSID_LEN] = "";
    char wifi_pass[WIFI_PASS_LEN] = "";
    if(wifi_user_config_handler(wifi_ssid, WIFI_SSID_LEN, wifi_pass, WIFI_PASS_LEN))
    {
        printf("Error configuring WiFi! Rebooting!\n");
        esp_restart();
    }
    printf("Connecting to '%s'\n", wifi_ssid);
    
    if(wifi_connect_sta(wifi_ssid, wifi_pass))
    {
        printf("WiFi connection failed! Rebooting!\n");
        esp_restart();
    }

    // get and build io configuration
    static io_config_t io_config = IO_CONFIG_DEFAULT();
    io_user_config_handler(&io_config);

    // setup and init IO
    ESP_ERROR_CHECK(setup_gpio_in(&io_config));
    ESP_ERROR_CHECK(setup_gpio_out(&io_config));
    ESP_ERROR_CHECK(setup_adc(&io_config));
    ESP_ERROR_CHECK(setup_dac(&io_config));

    // init modbus slave
    static modbus_data_t modbus_data;
    start_modbus_slave(&io_config, &modbus_data, app_netif);

    // start IO acquisition task
    static io_task_params_t io_task_params = {
        .io_config = &io_config,
        .modbus_data = &modbus_data
    };
    start_io_task(&io_task_params);

    // ready; do work async
    printf("up and running!\n");
    fflush(stdout);
}
