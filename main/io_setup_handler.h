#pragma once
#include "driver/i2s.h"
#include "driver/adc.h"
#include "hal/adc_ll.h" // providing SYSCON
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"


#define ADC_SAMPLE_RATE 200000
// using 16 sample buffer; should hold readings for all (max 8) channels
#define ADC_DMA_BUF_LEN 16


esp_err_t setup_gpio_in(io_config_t* io_config)
{
    const gpio_pullup_t pull_up = io_config->pull == UP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    const gpio_pulldown_t pull_down = io_config->pull == DOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    
    uint64_t pinmask = 0;
    for(int i = 0; i < count_discrete_in(io_config); i++)
    {
        printf("configuring GPIO %i as input\n", io_config->discrete_in[i]);
        pinmask |= (1 << io_config->discrete_in[i]);
    }

    gpio_config_t gpio_in_config = {
        .pin_bit_mask = pinmask,
        .pull_up_en = pull_up,
        .pull_down_en = pull_down,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    return gpio_config(&gpio_in_config);
}

esp_err_t setup_gpio_out(io_config_t* io_config)
{
    uint64_t pinmask = 0;
    for(int i = 0; i < count_coils(io_config); i++)
    {
        printf("configuring GPIO %i as output\n", io_config->coils[i]);
        pinmask |= (1 << io_config->coils[i]);
    }

    gpio_config_t gpio_out_config = {
        .pin_bit_mask = pinmask,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    return gpio_config(&gpio_out_config);
}

esp_err_t setup_dac(io_config_t* io_config)
{
    for(int i = 0; i < count_holding_reg(io_config); i++)
    {
        printf("configuring GPIO %i as DAC channel %i output\n", io_config->holding_reg[i], io_config->holding_reg_dac_channel[i] + 1);
        esp_err_t err_c = dac_output_enable(io_config->holding_reg_dac_channel[i]);
        esp_err_t err_s = dac_output_voltage(io_config->holding_reg_dac_channel[i], 0);
        
        if(err_c || err_s) { return err_c ? err_c : err_s; }
    }

    return ESP_OK;
}

// QueueHandle_t i2s_event_queue;
// void vAdcDmaTask(void* params)
// {
//     printf("adc dma reader task running!\n");
//     while(true) {
//         i2s_event_t evt;
//         if(xQueueReceive(i2s_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
//             switch(evt.type) {
//                 case I2S_EVENT_RX_DONE:;
//                     adc_digi_output_data_t adc_data[ADC_DMA_BUF_LEN];
//                     size_t adc_data_size = 0;
//                     i2s_read(I2S_NUM_0, adc_data, ADC_DMA_BUF_LEN * sizeof(adc_digi_output_data_t), &adc_data_size, portMAX_DELAY);
//                     /* DO SOMETHING */
//                     break;
//                 case I2S_EVENT_DMA_ERROR:
//                     printf("i2s DMA error!\n");
//                     break;
//                 default:
//                     printf("unknown i2s queue event\n");
//                     break;
//             }
//         }
//         else
//         {
//             printf("reading from adc dma event queue failed!\n");
//         }
//     }
// }

esp_err_t setup_adc(io_config_t* io_config)
{
    // count adc channels
    size_t count = count_input_reg(io_config);
    // size_t count = 0;
    // for(int8_t* in_reg_ptr = &(io_config->input_reg[0]); *in_reg_ptr != GPIO_NUM_NC; in_reg_ptr++)
    // {
    //     count++;
    // }

    // exit if no input register from adc is configured
    if(!count)
    {
        printf("no input registers configured; skipping ADC setup...\n");
        return ESP_OK;
    }

    // init i2c dma controller to read from adc1
    // static QueueHandle_t i2s_event_queue;

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN,
        .sample_rate = ADC_SAMPLE_RATE,
        .bits_per_sample = 16,
        // .use_apll = 0,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 2,
        .dma_buf_len = ADC_DMA_BUF_LEN // max 8 channels @ 16bits
    };

    // i2s_event_queue = xQueueCreate(1, sizeof(adc_digi_output_data_t));
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); //1, &i2s_event_queue);

    // configure adc1 with multiplex pattern table
    adc_digi_pattern_table_t* adc_pattern_tbl = malloc(sizeof(adc_digi_pattern_table_t) * count);
    adc_digi_pattern_table_t tbl_base = {
        .atten = ADC_ATTEN_DB_11,
        .bit_width = ADC_WIDTH_BIT_12,
    };
    for(size_t i = 0; i < count; i++)
    {
        printf("building table and initializing ADC1 channel %i\n", io_config->input_reg_adc_channel[i]);
        adc_digi_pattern_table_t* tbl = adc_pattern_tbl + i;
        *tbl = tbl_base;
        tbl->channel = io_config->input_reg_adc_channel[i];
        adc_gpio_init(ADC_UNIT_1, io_config->input_reg_adc_channel[i]);
    }

    adc_digi_config_t dig_cfg = {
        .conv_limit_en = true,
        .conv_limit_num = 255,
        .format = ADC_DIGI_FORMAT_12BIT,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .adc2_pattern_len = 0,
        .adc2_pattern = NULL
    };
    dig_cfg.adc1_pattern_len = count;
    dig_cfg.adc1_pattern = adc_pattern_tbl;
    // dig_cfg.conv_limit_num = count * 16;

    adc_power_acquire(); // possibly redundant
    adc_digi_init();
    adc_digi_controller_config(&dig_cfg);

    // start dma and adc
    i2s_start(I2S_NUM_0);
    // i2s_set_sample_rates(I2S_NUM_0, i2s_config.sample_rate);

    // wait for dma init; set adc freerunning without end of conversion
    vTaskDelay(10/portTICK_RATE_MS);
    SYSCON.saradc_ctrl2.meas_num_limit = 0;

    // start dma reader task
    // TaskHandle_t xAdcDma = NULL;
    // xTaskCreate(vAdcDmaTask, "adc_dma", 2048, NULL, tskIDLE_PRIORITY, &xAdcDma);
    // configASSERT(xAdcDma);

    // done
    return ESP_OK;
}

#define DEFAULT_VREF 1100
void get_adc_cal(esp_adc_cal_characteristics_t* adc_cal)
{
    // get adc calibration data
    switch(
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_cal)
    ){
        case ESP_ADC_CAL_VAL_EFUSE_VREF:
            printf("using ADC1 reference voltage calibration\n");
            break;
        case ESP_ADC_CAL_VAL_EFUSE_TP:
            printf("using ADC1 two point calibration\n");
            break;
        case ESP_ADC_CAL_VAL_DEFAULT_VREF :
            printf("using ADC1 default vref calibration\n");
            break;
        default:
            printf("unknown calibration for ADC1\n");
    }
}