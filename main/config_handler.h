#pragma once
#include "nvs_flash.h"
#include "io_helpers.h"
#include "wifi_handler.h"
#include "config_server.h"
#include "io_config.h"

#define NVS_STORAGE_NAMESPACE "storage"
#define CONFIG_KEY_TIMEOUT_SEC 1

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        printf("OPEN");
        break;
    case WIFI_AUTH_WEP:
        printf("WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        printf("WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        printf("WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        printf("WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        printf("WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        printf("WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        printf("WPA2_WPA3_PSK");
        break;
    default:
        printf("UNKNOWN");
        break;
    }
}

int wifi_user_config_handler(char* ssid, size_t ssid_len, char* passwd, size_t passwd_len)
{
    bool config_req = false;

    memset(ssid, 0, ssid_len*sizeof(char));
    memset(passwd, 0, passwd_len*sizeof(char));

    nvs_handle_t NVS;
    if (nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &NVS) != ESP_OK) {
        printf("Error opening NVS handle!\n");
        return EXIT_FAILURE;
    }
    // check for persistent ssid (and passwd) -> if none: req=true
    {
        size_t ssid_nvm_size;
        esp_err_t err = nvs_get_str(NVS, "ssid", NULL, &ssid_nvm_size);
        // printf("ssid size in nvm: %i\n", ssid_nvm_size);
        if(err != ESP_OK || ssid_nvm_size == 0)
        {
            // no persistent configuration present; require config
            printf("No persistent WiFi configuration present. Requiring config!");
            if(ssid_nvm_size == 0) { printf(" [size is 0]"); }
            config_req = true;
            printf("\n");
        }
    }

    // allow user to request configuration (if credentials present)
    if(!config_req)
    {
        printf("Press any key to configure WiFi...\n");
        fflush(stdout);

        // wait for config request
        for (int wait_config_reps = 10; wait_config_reps >= 0; wait_config_reps--) {
            if(fgetc(stdin) != EOF) {
                config_req = true;
                break;
            }
            vTaskDelay(CONFIG_KEY_TIMEOUT_SEC * 100 / portTICK_PERIOD_MS);
        }
    }

    if(config_req) {
        // let user configure wifi
        clear_stdin();
        
        printf("searching for WiFis...\n");
        wifi_ap_record_t wifi_list[WIFI_SCAN_LIST_SIZE];
        uint16_t wifi_count = WIFI_SCAN_LIST_SIZE;
        wifi_scan(&wifi_count, wifi_list);

        uint16_t wifi_count_proc = (wifi_count < WIFI_SCAN_LIST_SIZE ? wifi_count : WIFI_SCAN_LIST_SIZE);
        printf("Showing %i of %i SSIDs\n", wifi_count_proc, wifi_count);
        for (int i = 0; i < wifi_count_proc; i++) {
            printf("%i) %s [", i+1, wifi_list[i].ssid);
            print_auth_mode(wifi_list[i].authmode);
            printf("]\n");
        }

        char ssid_id_c[8] = "";
        int ssid_id = 0;
        do
        {
            printf("Select SSID (1-%i): ", wifi_count_proc);
            fgets_async_blocking(ssid_id_c, 8, stdin, true, false);
            ssid_id = atoi(ssid_id_c);
            printf("\n");
        } while(ssid_id < 1 || ssid_id > wifi_count_proc);
        ssid_id--; // adjust for indexing
        strncpy(ssid, (char*)wifi_list[ssid_id].ssid, ssid_len);
        
        if(wifi_list[ssid_id].authmode != WIFI_AUTH_OPEN)
        {
            do
            {
                printf("Enter passphrase:");
                fgets_async_blocking(passwd, passwd_len, stdin, true, true);
                printf("\n");
            } while(passwd[0] == 0);
        }
        clear_stdin();

        // persist values
        if(nvs_set_str(NVS, "ssid", ssid) != ESP_OK) { return EXIT_FAILURE; }
        if(nvs_set_str(NVS, "passwd", passwd) != ESP_OK) { return EXIT_FAILURE; }
        nvs_commit(NVS);
    }
    else
    {
        // use persisted values
        if(nvs_get_str(NVS, "ssid", ssid, &ssid_len) != ESP_OK) { return EXIT_FAILURE; }
        if(nvs_get_str(NVS, "passwd", passwd, &passwd_len) != ESP_OK) { return EXIT_FAILURE; }
    }
    return EXIT_SUCCESS;
}

#define IO_JSON_BUFF_SIZE 500
int io_user_config_handler(io_config_t* io_config)
{
    bool config_req = false;

    char io_json[IO_JSON_BUFF_SIZE];

    nvs_handle_t NVS;
    if (nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &NVS) != ESP_OK) {
        printf("Error opening NVS handle!\n");
        return EXIT_FAILURE;
    }
    // check for persistent io config -> if none: req=true
    {
        size_t io_json_nvm_size;
        esp_err_t err = nvs_get_blob(NVS, "io_json", NULL, &io_json_nvm_size);
        if(err != ESP_OK || io_json_nvm_size == 0 || io_json_nvm_size > IO_JSON_BUFF_SIZE)
        {
            // no persistent configuration present; require config
            printf("No valid persistent IO configuration present. Requiring config!");
            if(io_json_nvm_size == 0) { printf(" [size is 0]"); }
            if(io_json_nvm_size > IO_JSON_BUFF_SIZE) { printf(" [too large]"); }
            config_req = true;
            printf("\n");
        }
    }

    // allow user to request configuration (if credentials present)
    if(!config_req)
    {
        printf("Press any key to configure IO...\n");
        fflush(stdout);

        // wait for config request
        for (int wait_config_reps = 10; wait_config_reps >= 0; wait_config_reps--) {
            if(fgetc(stdin) != EOF) {
                config_req = true;
                break;
            }
            vTaskDelay(CONFIG_KEY_TIMEOUT_SEC * 100 / portTICK_PERIOD_MS);
        }
    }

    if(config_req) {
        size_t io_json_size = IO_JSON_BUFF_SIZE;
        get_io_json_http(io_json, &io_json_size);

        printf("got valid config!");

        // persist values
        if(nvs_set_blob(NVS, "io_json", io_json, io_json_size) != ESP_OK)
        {
            printf("storing IO config in nvs failed!\n");
            return EXIT_FAILURE;
        }
        nvs_commit(NVS);
    }
    else
    {
        // use persisted values
        size_t io_json_nvm_size;
        nvs_get_blob(NVS, "io_json", NULL, &io_json_nvm_size);
        // size has been tested to fit in allocated buffer before
        // char* io_json = malloc(io_json_nvm_size);
        if(nvs_get_blob(NVS, "io_json", io_json, &io_json_nvm_size) != ESP_OK)
        {
            printf("fetching IO config from nvs failed!\n");
            return EXIT_FAILURE;
        }
    }
    

    // build config
    printf("using config:\n%s\n", io_json);
    io_config_generate(io_json, io_config);

    return EXIT_SUCCESS;
}

void clear_user_config_handler()
{
        printf("Press any key to clear config...\n");
        fflush(stdout);
        bool clear_req = false;
        // wait for config request
        for (int wait_config_reps = 10; wait_config_reps >= 0; wait_config_reps--) {
            if(fgetc(stdin) != EOF) {
                clear_req = true;
                break;
            }
            vTaskDelay(CONFIG_KEY_TIMEOUT_SEC * 100 / portTICK_PERIOD_MS);
        }

        if(clear_req)
        {
            printf("Erasing user config!\n");
            nvs_handle_t NVS;
            if (nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &NVS) != ESP_OK) {
                printf("Error opening NVS handle!\n");
                return;
            }
            nvs_erase_all(NVS);
            nvs_commit(NVS);
        }
}