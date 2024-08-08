# BMP 581 Programming

Use the BMP5_api from Bosch. Check examples in the BMP5_api. 
They are general and not made for Arduino IDE but show how its done.
You can update BMP5_api by copying its contents from BOSH https://github.com/boschsensortec/BMP5_SensorAPI to src/bmp5_api folder (do not copy examples)

## Sparkfun Device Driver

Summarized from exampple programs in Bosch api distribution.

### Preparation of Device Interface

- interface init
    - set dev_addr_pin / CS_pin
    - set read function to custom readRegister
    - set write function to custom writeRegister
    - set intf to SPI
    - set dev address pin to output and low. This is integrated in readRegister and writeRegister and does not need to be done here.
    - set spi (bus, speed 7.5MHz, MODE0)

### Init the Sensor
Sparkfun driver does reset first. This has issue as the reset function does not sendy dummy read first which is necessary for the sensor to switch to SPI mode.
Correct approach according to examples from Bosch api is to use init as first command. Bosch examples do not use reset function, its not clear how and when it should be used.

Init usually fails at power up check. This checks non volatile memory ready and error. Perhas writting custom content to it at first use will solve the issue.

- bmp5_init

### Configure the Sensor

From Bosch example forced read.

- set power mode BMP5_POWERMODE_STANDBY
    
- press_en = BMP5_ENABLE with bmp5_set_osr_odr

- configure iir filter
    - iir_flush_forced_en
    - set_iir_t
    - set_iir_p
    - shw_set_iir_t
    - shdw_set_iir_p
    - bmp5_set_iir_config(set_iir_cfg)
    
- configure interrupts
    - bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH, BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, dev)
    - int_source_select.drdy_en = BMP5_ENABLE;
    - bmp5_int_source_select(&int_source_select, dev);
                
### Finally
- get sensor data

