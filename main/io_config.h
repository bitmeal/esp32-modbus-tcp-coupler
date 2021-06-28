#pragma once
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "soc/adc_channel.h"
#include "driver/dac.h"
#include "soc/dac_channel.h"
#include "cJSON.h"


typedef enum {
    OFF,
    UP,
    DOWN
} pull_resistor_t;

void print_pull_resistor(pull_resistor_t pull)
{
    switch(pull) {
        case OFF:
            printf("OFF");
            break;
        case UP:
            printf("UP");
            break;
        case DOWN:
            printf("DOWN");
            break;
        default:
            printf("UNKNOWN/OFF");
    }
}

#define COILS_MAX 32
#define DISCRETE_IN_MAX 32
#define HOLDING_REG_MAX 16
#define INPUT_REG_MAX 16
#define DISCRETE_GPIO_PIN_NUM_MAX 31

// max 32 coil/discrete and 16 register IO (registers physically limited to 2/8)
// pin arrays are initialized to PIN_NUM_NC (-1)
// register channels initialized to DAC_CHANNEL_MAX and ADC1_CHANNEL_MAX; input registers from ADC1 only!
typedef struct io_config_t {
    pull_resistor_t pull;
    int8_t coils[COILS_MAX];
    int8_t discrete_in[DISCRETE_IN_MAX];
    int8_t holding_reg[HOLDING_REG_MAX];
    dac_channel_t holding_reg_dac_channel[HOLDING_REG_MAX];
    int8_t input_reg[INPUT_REG_MAX];
    adc1_channel_t input_reg_adc_channel[INPUT_REG_MAX];
} io_config_t;

// #define IO_CONFIG_DEFAULT() { .pull = OFF, .coils = {GPIO_NUM_NC}, .discrete_in = {GPIO_NUM_NC}, .holding_reg = {GPIO_NUM_NC}, .input_reg = {GPIO_NUM_NC} }
#define IO_CONFIG_DEFAULT() \
    { \
        .pull = OFF, \
        .coils = {GPIO_NUM_NC}, \
        .discrete_in = {GPIO_NUM_NC}, \
        .holding_reg = {GPIO_NUM_NC}, \
        .holding_reg_dac_channel = {DAC_CHANNEL_MAX}, \
        .input_reg = {GPIO_NUM_NC}, \
        .input_reg_adc_channel = {ADC1_CHANNEL_MAX} \
    }
#define IO_CONFIG_INIT(io_config) \
    memset((io_config).coils, GPIO_NUM_NC, COILS_MAX); \
    memset((io_config).discrete_in, GPIO_NUM_NC, DISCRETE_IN_MAX); \
    memset((io_config).holding_reg, GPIO_NUM_NC, HOLDING_REG_MAX); \
    memset((io_config).holding_reg_dac_channel, DAC_CHANNEL_MAX, sizeof(dac_channel_t) * HOLDING_REG_MAX); \
    memset((io_config).input_reg, GPIO_NUM_NC, INPUT_REG_MAX) ; \
    memset((io_config).input_reg_adc_channel, ADC1_CHANNEL_MAX, sizeof(adc1_channel_t) * INPUT_REG_MAX)

uint8_t count_assigned_functions(int8_t* pin_config, size_t max_len)
{
    size_t count = 0;
    while(pin_config[count] != GPIO_NUM_NC && count < max_len)
    {
        count++;
    }
    return count;
}

uint8_t count_coils(io_config_t* io_config)
{
    return count_assigned_functions(io_config->coils, COILS_MAX);
}

uint8_t count_discrete_in(io_config_t* io_config)
{
    return count_assigned_functions(io_config->discrete_in, DISCRETE_IN_MAX);
}

uint8_t count_holding_reg(io_config_t* io_config)
{
    return count_assigned_functions(io_config->holding_reg, HOLDING_REG_MAX);
}

uint8_t count_input_reg(io_config_t* io_config)
{
    return count_assigned_functions(io_config->input_reg, INPUT_REG_MAX);
}

void print_gpio_arr(int8_t* arr, size_t max)
{
    if(arr[0] != GPIO_NUM_NC)
    {
        printf("%i", *arr);
        for(int i = 1; *(arr + i) != GPIO_NUM_NC && i < max; ++i)
        {
            printf(", %i", *(arr + i));
        }
    }
}

void io_config_print(io_config_t* io_config)
{
    printf("pull: ");
    print_pull_resistor(io_config->pull);
    printf("\n");

    printf("coils: ");
    print_gpio_arr(io_config->coils, COILS_MAX);
    printf("\n");

    printf("discrete inputs: ");
    print_gpio_arr(io_config->discrete_in, DISCRETE_IN_MAX);
    printf("\n");

    printf("holding registers: ");
    print_gpio_arr(io_config->holding_reg, HOLDING_REG_MAX);
    printf("\n");

    printf("input registers: ");
    print_gpio_arr(io_config->input_reg, INPUT_REG_MAX);
    printf("\n");
}

// check for multiple pin use, GPIO out of bounds, and ADC/DAC pin/channel assignment
esp_err_t io_config_validate(io_config_t* io_config)
{
    bool has_err = false;

    uint64_t coil_gpio = 0x00;
    uint64_t discrete_in_gpio = 0x00;
    uint64_t holding_reg_gpio = 0x00;
    uint64_t input_reg_gpio = 0x00;

    // check DAC channels
    {
        int8_t* holding_reg = io_config->holding_reg;
        dac_channel_t* holding_reg_dac_channel = io_config->holding_reg_dac_channel;
        while(*holding_reg != GPIO_NUM_NC && holding_reg != (&(io_config->holding_reg[HOLDING_REG_MAX - 1]) + 1))
        {
            holding_reg_gpio |= (1 << *holding_reg);

            gpio_num_t pin;
            esp_err_t err = dac_pad_get_io_num(*holding_reg_dac_channel, &pin);
            if(err || pin != *holding_reg)
            {
                printf("holding register DAC channel on GPIO %i does not match connected DAC channel!", *holding_reg);
                has_err = true;
            }
            holding_reg++;
            holding_reg_dac_channel++;
        }
    }    

    // check ADC channels
    {
        int8_t* input_reg = io_config->input_reg;
        adc1_channel_t* input_reg_adc_channel = io_config->input_reg_adc_channel;
        while(*input_reg != GPIO_NUM_NC && input_reg != (&(io_config->input_reg[INPUT_REG_MAX - 1]) + 1))
        {
            input_reg_gpio |= (1 << *input_reg);

            gpio_num_t pin;
            esp_err_t err = adc1_pad_get_io_num(*input_reg_adc_channel, &pin);
            if(err || pin != *input_reg)
            {
                printf("input register ADC channel on GPIO %i does not match connected ADC channel!", *input_reg);
                has_err = true;
            }
            input_reg++;
            input_reg_adc_channel++;
        }
    }

    // get coils GPIOs and check out of GPIO bounds
    {
        // int8_t* coil = &(io_config->coils[0]);
        // while(*coil != GPIO_NUM_NC && coil != (&(io_config->coils[COILS_MAX - 1]) + 1))
        for(int8_t* coil = io_config->coils; *coil != GPIO_NUM_NC && coil != (&(io_config->coils[COILS_MAX - 1]) + 1); coil++)
        {
            coil_gpio |= (1 << *coil);

            if(*coil > DISCRETE_GPIO_PIN_NUM_MAX)
            {
                printf("coil on GPIO %i out of bounds; max GPIO number is %i", *coil, DISCRETE_GPIO_PIN_NUM_MAX);
                has_err = true;
            }
        }
    }

    // get discrete in GPIOs and check out of GPIO bounds
    {
        // int8_t* discrete_in = &(io_config->discrete_in[0]);
        // while(*discrete_in != GPIO_NUM_NC && discrete_in != (&(io_config->discrete_in[DISCRETE_IN_MAX - 1]) + 1))
        for(int8_t* discrete_in = io_config->discrete_in; *discrete_in != GPIO_NUM_NC && discrete_in != (&(io_config->discrete_in[DISCRETE_IN_MAX - 1]) + 1); discrete_in++)
        {
            discrete_in_gpio |= (1 << *discrete_in);

            if(*discrete_in > DISCRETE_GPIO_PIN_NUM_MAX)
            {
                printf("discrete in on GPIO %i out of bounds; max GPIO number is %i", *discrete_in, DISCRETE_GPIO_PIN_NUM_MAX);
                has_err = true;
            }
        }
    }

    // check GPIO overlap
    if(coil_gpio & discrete_in_gpio & holding_reg_gpio & input_reg_gpio)
    {
        printf("requested GPIO pins are overlapping!");
        has_err = true;
    }

    return has_err ? ESP_FAIL : ESP_OK;
}

// CONFIG JSON:
// {
//     "pull": "up/down",
//     "discrete_in": [1, 2],
//     "coils": [11, 12],
//     "holding_reg": [25, 26],
//     "input_reg": [34, 35]
// }

// generator checks for size constrains and pin assignment resolution while building config
// resulting config as a whole is automatically checked with io_config_validate() after generation
esp_err_t io_config_generate(char* io_json, io_config_t* io_config)
{
    bool has_err = false;

    IO_CONFIG_INIT(*io_config);
    cJSON* root = cJSON_Parse(io_json);
    if(!root)
    {
        printf("Could not parse IO config JSON!\n");
        return ESP_FAIL;
    }
    
    // pull resistors
    cJSON* pull = cJSON_GetObjectItem(root, "pull");
    if(pull)
    {
        if(strcmp("up", pull->valuestring) == 0)
        {
            io_config->pull = UP;
        }
        else if(strcmp("down", pull->valuestring) == 0)
        {
            io_config->pull = DOWN;
        }
    }
    else
    {
        io_config->pull = OFF;
    }

    // coils
    cJSON* coils = cJSON_GetObjectItem(root, "coils");
    if(coils && cJSON_IsArray(coils))
    {
        if(cJSON_GetArraySize(coils) <= sizeof(io_config->coils))
        {
            int8_t* coil_store = io_config->coils;
            cJSON* coil;
            cJSON_ArrayForEach(coil, coils)
            {
                if(cJSON_IsNumber(coil))
                {
                    *coil_store = coil->valueint;
                    coil_store++;
                    printf("added coil on GPIO %i\n", coil->valueint);
                }
                else
                {
                    printf("skipping non-number entry in \"coils\"!\n");
                    has_err = true;
                }
            }
        }
        else
        {
            printf("too many pins for coils!\n");
            has_err = true;
        }
    }

    // discrete ins
    cJSON* discrete_ins = cJSON_GetObjectItem(root, "discrete_in");
    if(discrete_ins && cJSON_IsArray(discrete_ins))
    {
        if(cJSON_GetArraySize(discrete_ins) <= sizeof(io_config->discrete_in))
        {
            int8_t* discrete_in_store = io_config->discrete_in;
            cJSON* discrete_in;
            cJSON_ArrayForEach(discrete_in, discrete_ins)
            {
                if(cJSON_IsNumber(discrete_in))
                {
                    *discrete_in_store = discrete_in->valueint;
                    discrete_in_store++;
                    printf("added discrete in on GPIO %i\n", discrete_in->valueint);
                }
                else
                {
                    printf("skipping non-number entry in \"discrete_in\"!\n");
                    has_err = true;
                }
            }
        }
        else
        {
            printf("too many pins for discrete inputs!\n");
            has_err = true;
        }
    }

    // holding registers
    cJSON* holding_regs = cJSON_GetObjectItem(root, "holding_reg");
    if(holding_regs && cJSON_IsArray(holding_regs))
    {
        if(cJSON_GetArraySize(holding_regs) <= sizeof(io_config->holding_reg))
        {
            int8_t* holding_reg_store = io_config->holding_reg;
            dac_channel_t* holding_reg_dac_channel_store = io_config->holding_reg_dac_channel;
            cJSON* holding_reg;
            cJSON_ArrayForEach(holding_reg, holding_regs)
            {
                if(cJSON_IsNumber(holding_reg))
                {
                    switch(holding_reg->valueint)
                    {
                        case DAC_CHANNEL_1_GPIO_NUM:
                            *holding_reg_dac_channel_store = DAC_CHANNEL_1;
                            holding_reg_dac_channel_store++;
                            printf("added holding register out for DAC channel %i on GPIO %i\n", 1, holding_reg->valueint);
                            break;
                        case DAC_CHANNEL_2_GPIO_NUM:
                            *holding_reg_dac_channel_store = DAC_CHANNEL_2;
                            holding_reg_dac_channel_store++;
                            printf("added holding register out for DAC channel %i on GPIO %i\n", 2, holding_reg->valueint);
                            break;
                        default:
                            printf("GPIO %i not connected to a DAC channel as holding register!", holding_reg->valueint);
                            has_err = true;
                            // CONTINUE LOOP WITH NEXT PIN
                            continue;
                    }
                    
                    *holding_reg_store = holding_reg->valueint;
                    holding_reg_store++;
                }
                else
                {
                    printf("skipping non-number entry in \"holding_reg\"!\n");
                    has_err = true;
                }
            }
        }
        else
        {
            printf("too many pins for holding registers!\n");
            has_err = true;
        }
    }

#define HANDLE_CASE_ADC1_CHANNEL(n) \
    case ADC1_CHANNEL_ ## n ## _GPIO_NUM: \
        *input_reg_adc_channel_store = ADC1_CHANNEL_ ## n; \
        input_reg_adc_channel_store++; \
        printf("added input register in for ADC1 channel %i on GPIO %i\n",n , input_reg->valueint); \
        break

    // input registers
    cJSON* input_regs = cJSON_GetObjectItem(root, "input_reg");
    if(input_regs && cJSON_IsArray(input_regs))
    {
        if(cJSON_GetArraySize(input_regs) <= sizeof(io_config->input_reg))
        {
            int8_t* input_reg_store = io_config->input_reg;
            adc1_channel_t* input_reg_adc_channel_store = io_config->input_reg_adc_channel;
            cJSON* input_reg;
            cJSON_ArrayForEach(input_reg, input_regs)
            {
                if(cJSON_IsNumber(input_reg))
                {
                    switch(input_reg->valueint)
                    {
                        HANDLE_CASE_ADC1_CHANNEL(0);
                        HANDLE_CASE_ADC1_CHANNEL(1);
                        HANDLE_CASE_ADC1_CHANNEL(2);
                        HANDLE_CASE_ADC1_CHANNEL(3);
                        HANDLE_CASE_ADC1_CHANNEL(4);
                        HANDLE_CASE_ADC1_CHANNEL(5);
                        HANDLE_CASE_ADC1_CHANNEL(6);
                        HANDLE_CASE_ADC1_CHANNEL(7);
                        default:
                            printf("GPIO %i not connected to a ADC1 channel as input register!", input_reg->valueint);
                            has_err = true;
                            // CONTINUE LOOP WITH NEXT PIN
                            continue;
                    }
                    
                    *input_reg_store = input_reg->valueint;
                    input_reg_store++;
                }
                else
                {
                    printf("skipping non-number entry in \"input_reg\"!\n");
                    has_err = true;
                }
            }
        }
        else
        {
            printf("too many pins for input registers!\n");
            has_err = true;
        }
    }

    if(!(coils || discrete_ins || holding_regs || input_regs))
    {
        printf("No suitable keys found in IO configuration!\n");
        return ESP_FAIL;
    }

    if(has_err || io_config_validate(io_config))
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}
