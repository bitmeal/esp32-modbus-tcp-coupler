# esp32 modbus/tcp coupler
Transform an ESP32 into a simple Modbus/TCP bus-coupler/IO-device. Use any GPIO as *coil* or *discrete input* with configurable pull resistor. Use up to 8 onboard ADC and two onboard DAC channels as analog IO from *input-* or *holding registers*. IOs are sampled in a fixed frequency realtime loop, with single digit microsecond jitter, and nano- to microseconds simultaneity of IO operations (see IO section below for details).

## quick-start
0. build & flash with ESP-IDF toolchain
1. connect to serial console
2. perform WiFi configuration using serial console
3. *POST* an IO configuration to `http://<esp32-ip-address>/config`
4. 🎉

### example IO configuration
```json
{
    "pull": "down",
    "discrete_in": [14, 12],
    "coils": [22, 23],
    "input_reg": [35, 34],
    "holding_reg": [25, 26]
}
```
All IOs are configured by using their GPIO Number! For *input registers* (ADC) and *holding registers* (DAC), be sure to select a pin that is connected to the internal DAC or one of the **ADC1** channels!

*Pins will be checked for requested function and configuration will fail if unsupported.*


Available configuration fields: ***array of GPIO numbers to use as*** ...
* `pull`: enable pull resistors on digital inputs (`up`, `down`, *omit*)
* `discrete_in`: [...] digital **in**puts
* `coils`: [...] digital **out**puts
* `input_reg`: [...] analog input channels for **ADC1**
* `holding_reg`: [...] analog output channels

## Modbus/TCP
* Unit/Device `1`
* Port `502`

## IO info
* All IOs/registers start at address 0
* All "register-IOs" use one register (16 bits) each
* IO data is read and written in a realtime loop with *100Hz*
* Digital IO and analog out (DAC) are read and written within < 2µs
* Analog input timing: *see below*


### input registers
* 16 bit wide (`WORD`)
* to be interpreted as unsigned integer value
* payload data width 12 bits
* raw ADC readings @ 11db internal attenuation

Analog input channels are sampled consecutively at *200kHz* in two DMA buffers holding 16 samples each. Under normal operation all channels should be sampled in order and the time difference between samples for different channels should be lower than `(<channel_count> - 1)/200kHz` (`^= 35µs` @ 8 channels). Maximum age of analog readings should be lower than `2 * 16 / 200kHz ^= 160µs`.

### holding registers
* 16 bit wide (`WORD`)
* only 8 bits of payload as **unsigned integer**
* raw DAC output values (*0-255*)


## building & optimization
For consistent and fast sample rates and least IO-loop-jitter, configure a high CPU clock and high RTOS tick rate. A `sdkconfig.defaults` is provided and should set the following parameters accordingly:
```ini
#sdkconfig.defaults

CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=240

CONFIG_FREERTOS_HZ=1000
```
