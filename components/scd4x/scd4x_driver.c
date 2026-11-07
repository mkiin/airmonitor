#include "scd4x_driver.h"

#include <stddef.h>

#include "driver/i2c_master.h"

#define SCD41_ADDRESS  0x62
#define I2C_TIMEOUT_MS 100

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;

static esp_err_t scd4x_send_command(uint16_t command)
{
    if ( s_device == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t bytes[] = { (uint8_t)(command >> 8), (uint8_t)command };
    return i2c_master_transmit(s_device, bytes, sizeof(bytes), I2C_TIMEOUT_MS);
}

static uint8_t scd4x_crc(const uint8_t *bytes)
{
    uint8_t crc = 0xff;
    for ( size_t i = 0; i < 2; ++i )
    {
        crc ^= bytes[ i ];
        for ( unsigned bit = 0; bit < 8; ++bit )
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t scd4x_receive_words(uint16_t *words, size_t count)
{
    if ( s_device == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }
    if ( words == NULL || count == 0 || count > 3 )
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t   bytes[ 9 ];  // Three words, each followed by a CRC byte.
    esp_err_t err = i2c_master_receive(s_device, bytes, count * 3, I2C_TIMEOUT_MS);
    if ( err != ESP_OK )
    {
        return err;
    }

    for ( size_t i = 0; i < count; ++i )
    {
        const uint8_t *word = &bytes[ i * 3 ];
        if ( scd4x_crc(word) != word[ 2 ] )
        {
            return ESP_ERR_INVALID_CRC;
        }
        words[ i ] = ((uint16_t)word[ 0 ] << 8) | word[ 1 ];
    }
    return ESP_OK;
}

esp_err_t scd4x_driver_init(void)
{
    if ( s_bus != NULL || s_device != NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = GPIO_NUM_21,
        .scl_io_num                   = GPIO_NUM_47,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = false,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus);
    if ( err != ESP_OK )
    {
        return err;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SCD41_ADDRESS,
        .scl_speed_hz    = 100000,
    };
    err = i2c_master_bus_add_device(s_bus, &device_config, &s_device);
    if ( err != ESP_OK )
    {
        if ( i2c_del_master_bus(s_bus) == ESP_OK )
        {
            s_bus = NULL;
        }
    }
    return err;
}

esp_err_t scd4x_driver_deinit(void)
{
    if ( s_device != NULL )
    {
        esp_err_t err = i2c_master_bus_rm_device(s_device);
        if ( err != ESP_OK )
        {
            return err;
        }
        s_device = NULL;
    }
    if ( s_bus != NULL )
    {
        esp_err_t err = i2c_del_master_bus(s_bus);
        if ( err != ESP_OK )
        {
            return err;
        }
        s_bus = NULL;
    }
    return ESP_OK;
}

esp_err_t scd4x_driver_start_periodic_measurement(void)
{
    return scd4x_send_command(0x21b1);
}

esp_err_t scd4x_driver_stop_periodic_measurement(void)
{
    return scd4x_send_command(0x3f86);
}

esp_err_t scd4x_driver_request_data_ready(void)
{
    return scd4x_send_command(0xe4b8);
}

esp_err_t scd4x_driver_read_data_ready(bool *ready)
{
    if ( ready == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t  status;
    esp_err_t err = scd4x_receive_words(&status, 1);
    if ( err == ESP_OK )
    {
        *ready = (status & 0x07ff) != 0;
    }
    return err;
}

esp_err_t scd4x_driver_request_measurement(void)
{
    return scd4x_send_command(0xec05);
}

esp_err_t scd4x_driver_read_measurement(uint16_t *co2_ppm, int32_t *temperature, int32_t *humidity)
{
    if ( co2_ppm == NULL || temperature == NULL || humidity == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t  words[ 3 ];
    esp_err_t err = scd4x_receive_words(words, 3);
    if ( err == ESP_OK )
    {
        *co2_ppm     = words[ 0 ];
        *temperature = ((21875 * (int32_t)words[ 1 ]) >> 13) - 45000;
        *humidity    = (12500 * (int32_t)words[ 2 ]) >> 13;
    }
    return err;
}
