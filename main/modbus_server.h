#pragma once
#include "io_config.h"
#include "esp_modbus_slave.h"

// copy max sizes from io config data
typedef struct modbus_data_t {
    // int8_t coils[sizeof(io_config_t::coils) / 8 + 1];
    // int8_t discrete_in[sizeof(io_config_t::discrete_in) / 8 + 1];
    uint64_t coils;
    uint64_t discrete_in;
    uint16_t holding_reg[HOLDING_REG_MAX];
    uint16_t input_reg[INPUT_REG_MAX];
} modbus_data_t;

#define MB_TCP_PORT_NUMBER 502
esp_err_t start_modbus_slave(io_config_t* io_config, modbus_data_t* modbus_data, esp_netif_t* modbus_netif)
{
    // init and configure slave, and start modbus slave stack
    void* mbc_slave_handler = NULL;
    ESP_ERROR_CHECK(mbc_slave_init_tcp(&mbc_slave_handler));
    
    mb_communication_info_t comm_info = { 0 };
    comm_info.ip_port = MB_TCP_PORT_NUMBER;
    comm_info.ip_addr_type = MB_IPV4;
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = NULL;
    comm_info.ip_netif_ptr = (void*)modbus_netif;
    ESP_ERROR_CHECK(mbc_slave_setup((void*)&comm_info));
    
    // setup slave data descriptors
        // COILS
        mb_register_area_descriptor_t mb_coils_reg;
        mb_coils_reg.type = MB_PARAM_COIL;
        mb_coils_reg.start_offset = 0;
        mb_coils_reg.address = (void*)&(modbus_data->coils);
        mb_coils_reg.size = sizeof(modbus_data->coils);

        // DISCRETE IN
        mb_register_area_descriptor_t mb_discrete_in_reg;
        mb_discrete_in_reg.type = MB_PARAM_DISCRETE;
        mb_discrete_in_reg.start_offset = 0;
        mb_discrete_in_reg.address = (void*)&(modbus_data->discrete_in);
        mb_discrete_in_reg.size = sizeof(modbus_data->discrete_in);

        // HOLDING REGISTERS
        mb_register_area_descriptor_t mb_holding_reg;
        mb_holding_reg.type = MB_PARAM_HOLDING;
        mb_holding_reg.start_offset = 0;
        mb_holding_reg.address = (void*)modbus_data->holding_reg;
        mb_holding_reg.size = sizeof(modbus_data->holding_reg);

        // INPUT REGISTERS
        mb_register_area_descriptor_t mb_input_reg;
        mb_input_reg.type = MB_PARAM_INPUT;
        mb_input_reg.start_offset = 0;
        mb_input_reg.address = (void*)modbus_data->input_reg;
        mb_input_reg.size = sizeof(modbus_data->input_reg);

    // zero data
    memset((void*)&(modbus_data->coils), 0, sizeof(modbus_data->coils));
    memset((void*)&(modbus_data->discrete_in), 0, sizeof(modbus_data->discrete_in));
    memset((void*)modbus_data->holding_reg, 0, sizeof(modbus_data->holding_reg));
    memset((void*)modbus_data->input_reg, 0, sizeof(modbus_data->input_reg));

    // setup slave data
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(mb_coils_reg));
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(mb_discrete_in_reg));
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(mb_holding_reg));
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(mb_input_reg));

    // start slave
    printf("starting modbus slave...\n");
    ESP_ERROR_CHECK(mbc_slave_start());

    return ESP_OK;
}
