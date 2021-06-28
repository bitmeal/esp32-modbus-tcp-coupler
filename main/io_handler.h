#pragma once
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/dac.h"
#include "esp_adc_cal.h"
#include "io_setup_handler.h" // for dma buffer size
#include "modbus_server.h"


// data: pointer to array of sufficient size
// channel_data_mapping: array, mapping channel numbers as array indices to positions/indices in data array; set unused channels to ADC_CHANNEL_MAX
// channel_mask: bitmask, with bits at position of required channel set to 1
// as_mv: output millivolts instead of raw readings
esp_err_t read_adc(uint16_t* data, int8_t channel_data_mapping[8], uint8_t channel_mask)
{
    // skip if no actual channels are requested
    if(!channel_mask) { return ESP_OK; }

    adc_digi_output_data_t adc_data[ADC_DMA_BUF_LEN];
    size_t adc_data_size = 0;
    i2s_read(I2S_NUM_0, adc_data, ADC_DMA_BUF_LEN * sizeof(adc_digi_output_data_t), &adc_data_size, portMAX_DELAY);

    size_t adc_raw_count = adc_data_size/sizeof(adc_digi_output_data_t);
    uint8_t read_channel_mask = 0;

    // loop through raw readings from back(newest), until all are processed, or we found a match for all requested channels
    for(int i = adc_raw_count - 1; i >= 0 && read_channel_mask != channel_mask; i--)
    {
        uint8_t reading_mask = 1 << adc_data[i].type1.channel;
        // test if channel has already been read
        if(!(reading_mask & read_channel_mask))
        {
            // add to read channels
            read_channel_mask |= reading_mask;

            // test if channel is actually to be sampled, or something went wrong
            if(channel_data_mapping[adc_data[i].type1.channel] != ADC_CHANNEL_MAX)
            {
                // assign data to output data array
                *(data + channel_data_mapping[adc_data[i].type1.channel]) = adc_data[i].type1.data;
            }
            else
            {
                // printf("got reading for non-requested ADC channel #%i\n", adc_data[i].type1.channel);
            }
        }
    }

    // return successful acquisition of all requested channels
    return read_channel_mask == channel_mask ? ESP_OK : ESP_FAIL;
}

void write_dac(uint16_t* data, int8_t data_channel_mapping[2])
{
    for(int i = 0; data_channel_mapping[i] != DAC_CHANNEL_MAX && i < 2; i++)
    {
        RTCIO.pad_dac[data_channel_mapping[i]].dac = (uint8_t)*(data + i);
        // esp_err_t err = dac_output_voltage(data_channel_mapping[i], (uint8_t)*(data + i));
        // if(err) { return err; };
    }

    // return ESP_OK;
}

// always read both GPIO registers
void read_gpio_in(uint64_t* data, int8_t* pins, size_t count)
{
    *data = 0x00;
    uint64_t gpio_in = 0x00;
    gpio_in |= ((uint64_t)REG_READ(GPIO_IN1_REG) << 32);
    gpio_in |= REG_READ(GPIO_IN_REG);

    for(int i = 0; i < count; i++)
    {
        *data |= ((gpio_in >> *(pins + i)) & 0x01) << i;
    }
}

void gpio_out_build(uint64_t data, int8_t* pins, uint64_t mask, size_t count, uint64_t* mask_set, uint64_t* mask_clear)
{
    *mask_set = 0x00;
    *mask_clear = 0x00;

    // shift data at end of each iteration
    for(int i = 0; i < count; i++)
    {
        *mask_set |= (data & (uint64_t)0x01) << *(pins + i);

        // shift
        data >>= 1;
    }

    *mask_clear = mask & ~*mask_set;

    // printf("GPIO SET: %lli; GPIO CLEAR: %lli\n", *mask_set, mask_clear);

}

void gpio_out_latch(uint64_t mask_set, uint64_t mask_clear)
{
    REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t) (mask_set >> 32));
    REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t) mask_set);
    REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t) (mask_clear >> 32));
    REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t) mask_clear);
}

// always write both GPIO registers
void write_gpio_out(uint64_t data, int8_t* pins, uint64_t mask, size_t count)
{
    uint64_t mask_set, mask_clear;
    gpio_out_build(data, pins, mask, count, &mask_set, &mask_clear);
    gpio_out_latch(mask_set, mask_clear);
}

typedef struct io_task_params_t {
    io_config_t* io_config;
    modbus_data_t* modbus_data;
} io_task_params_t;

// uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_data[i].type1.data, &adc_cal);
// TODO: mark regions as critical? prevents task preemption
#define IO_TASK_RATE_MS 10
void vIOTask(void* params) // io_config_t* params
{
    // get parameters
    io_config_t* io_config = ((io_task_params_t*) params)->io_config;
    modbus_data_t* modbus_data = ((io_task_params_t*) params)->modbus_data;
    
    // setup loop
    const TickType_t xFrequency = IO_TASK_RATE_MS / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // setup IO data structures
        // coils
        const size_t coils_count = count_coils(io_config);
        uint64_t coils_mask = 0x00;
        for(int i = 0; i < coils_count; i++)
        {
            coils_mask |= (uint64_t) 0x01 << io_config->coils[i];
        }
        // uint64_t coils_data = 0;

        // holding registers
        const size_t holding_reg_count = count_holding_reg(io_config);
        // uint16_t holding_reg_data[2] = {0, 0};
        int8_t dac_data_channel_mapping[2] = {DAC_CHANNEL_MAX, DAC_CHANNEL_MAX};
        for(int i = 0; i < holding_reg_count; i++)
        {
            dac_data_channel_mapping[i] = io_config->holding_reg_dac_channel[i];
        }

        // discrete inputs
        const size_t discrete_in_count = count_discrete_in(io_config);
        // uint64_t discrete_in_data = 0;

        // input registers - ADC
        const size_t input_reg_count = count_input_reg(io_config);
        // uint16_t* input_reg_data = malloc(sizeof(uint16_t) * input_reg_count);
        int8_t adc_channel_data_mapping[8];
        memset((void*)adc_channel_data_mapping, ADC_CHANNEL_MAX, 8);
        uint8_t adc_channel_mask = 0;
        for(int i = 0; i < input_reg_count; i++)
        {
            adc_channel_mask |= (1 << io_config->input_reg_adc_channel[i]);
            adc_channel_data_mapping[io_config->input_reg_adc_channel[i]] = i;
        }

    while(true) {
        /* DO WORK */
        // PREPARE WRITE DATA (!!! MOCK !!!)
        // static int loop_counter = 0;
            // holding registers
            // uint8_t dac_out = (loop_counter * 16) % 255;
            // for(int i = 0; i < holding_reg_count; i++)
            // {
            //     // printf("setting DAC channel %i @ GPIO %i to %i", io_config->holding_reg_dac_channel[i], io_config->holding_reg[i], dac_out);
            //     holding_reg_data[i] = dac_out;
            // }

            // // coils
            // for(int i = 0; i < coils_count; i++)
            // {
            //     // toggle each loop
            //     coils_data ^= (uint64_t) 1 << i;
            // }
        // loop_counter++;

        // PREPARE
            uint64_t mask_set;
            uint64_t mask_clear;
            gpio_out_build(/*coils_data*/ modbus_data->coils, io_config->coils, coils_mask, coils_count, &mask_set, &mask_clear);
        // READ DATA
            // discrete inputs
            read_gpio_in(/*&discrete_in_data*/ &(modbus_data->discrete_in), io_config->discrete_in, discrete_in_count);
            // input registers / ADC
            /*esp_err_t adc_read_err = */
            read_adc(/*input_reg_data*/ modbus_data->input_reg, adc_channel_data_mapping, adc_channel_mask);

        // WRITE DATA 
            write_dac(/*holding_reg_data*/ modbus_data->holding_reg, dac_data_channel_mapping);
            gpio_out_latch(mask_set, mask_clear);
            // write_gpio_out(coils_data, &(io_config->coils[0]), coils_mask, coils_count);

        // PRINT DATA
            // // discrete in
            // printf("DISCRETE IN: ");
            // for(int i = 0; i < discrete_in_count; i++)
            // {
            //     printf("%-4s", (discrete_in_data & (1 << i) ? "ON" : "OFF"));
            // }
            // printf("\n");
            // // input registers
            // if(!adc_read_err)
            // {
            //     printf("INPUT REGISTERS: ");
            //     for(int i = 0; i < input_reg_count; i++)
            //     {
            //         printf("%5d%s", *(input_reg_data + i), (i == input_reg_count - 1 ? "\n" : ", "));
            //     }
            // }
            // else
            // {
            //     printf("error reading ADC!\n");
            // }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// TODO: adjust task priority
#define IO_TASK_STACK_SIZE 2048
void start_io_task(io_task_params_t* io_task_params)
{
    TaskHandle_t xIOTask = NULL;
    xTaskCreate(vIOTask, "io_task", IO_TASK_STACK_SIZE, (void*) io_task_params, tskIDLE_PRIORITY, &xIOTask);
    configASSERT(xIOTask);
}