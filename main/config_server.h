#pragma once
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_http_server.h"

#include "io_config.h"


typedef struct handler_ctx_t {
    char* data;
    size_t size;
    EventGroupHandle_t evtgrp;
} handler_ctx_t;

static esp_err_t config_post_handler(httpd_req_t *req)
{
    char* buffJSON = ((handler_ctx_t*) req->user_ctx)->data;
    size_t buffJSON_size = ((handler_ctx_t*) req->user_ctx)->size;
    // printf("Returning JSON to %p[%u]\n", buffJSON, buffJSON_size);

    char buf[100];
    int transferred = 0;
    int ret, remaining = req->content_len;
    // printf("content-length: %i\n", remaining);

    if(remaining > buffJSON_size)
    {
        printf("too much data!\n");
        char err_resp[128];
        sprintf(err_resp, "{\"error:\": \"buffer exhausted; use less than %i bytes\"}", buffJSON_size);
        printf("err resp: %s\n", err_resp);
        httpd_resp_send_chunk(req, err_resp, strlen(err_resp));
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        (remaining < sizeof(buf) ? remaining : sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        // transfer to accumulation buffer
        memcpy(buffJSON + transferred, buf, (remaining < sizeof(buf) ? remaining : sizeof(buf)));
        transferred += ret;
        remaining -= ret;
    }
    ((handler_ctx_t*) req->user_ctx)->size = strlen(buffJSON);
    // send response
    httpd_resp_send_chunk(req, NULL, 0);

    printf("=========== RECEIVED DATA ==========\n%s\n====================================\n", buffJSON);


    io_config_t io_config = IO_CONFIG_DEFAULT();
    if(!io_config_generate(buffJSON, &io_config))
    {
        io_config_print(&io_config);
        printf("receive OK; sending signal...\n");
        xEventGroupSetBits(((handler_ctx_t*) req->user_ctx)->evtgrp, BIT0);
    }
    else
    {
        printf("could not read config\n");
    }

    fflush(stdout);
    
    return ESP_OK;
}


esp_err_t get_io_json_http(char* io_json, size_t* io_json_size)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    EventGroupHandle_t io_json_event_group = xEventGroupCreate();

    handler_ctx_t json_ctx = {
        .data = io_json,
        .size = *io_json_size,
        .evtgrp = io_json_event_group
    };

    httpd_uri_t config_uri = {
        .uri       = "/config",
        .method    = HTTP_POST,
        .handler   = config_post_handler
        // .user_ctx  = (void*)&json_ctx
    };
    config_uri.user_ctx  = (void*)&json_ctx;


    // Start the httpd server
    printf("Starting server on port: '%d'\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        // printf("Registering URI handlers\n");
        httpd_register_uri_handler(server, &config_uri);
    }
    else
    {
        printf("Configuration server creation failed!\n");
        return ESP_FAIL;
    }

    // wait async blocking for config data
    EventBits_t bits;
    do
    {
        printf("waiting for IO config...\n");
        bits = xEventGroupWaitBits(io_json_event_group, BIT0, pdFALSE, pdFALSE, portMAX_DELAY);
    } while(!(bits & BIT0));
    vEventGroupDelete(io_json_event_group);

    // copy size from handler to result
    *io_json_size = json_ctx.size;

    printf("stopping server...\n");
    httpd_stop(server);

    return ESP_OK;
}