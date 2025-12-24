#include "pump.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2c_master.h"

#define PUMP_BUFFER_SIZE 41
#define PUMP_PROCESSING_DELAY 300 // milliseconds
#define PUMP_MAX_ATTEMPTS 2

static const char *TAG = "pump";

#define PUMP_ERROR_RETURN(err_msg) do { \
    last_error = (err_msg); \
    ESP_LOGE(TAG, "%s", err_msg); \
    return NULL; \
} while(0)

static char *last_error = NULL;

const char* pump_get_last_error() {
    return last_error;
}

typedef struct {
    settings_t *settings;
    char buf[PUMP_BUFFER_SIZE];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
} pump_context_t;

pump_context_t* pump_init(settings_t *settings, httpd_handle_t server) {
    if (settings->pump_scl_gpio < 0 || settings->pump_sda_gpio < 0) {
        PUMP_ERROR_RETURN("Pump initialization skipped because weight sensor GPIOs are not configured");
    }

    ESP_LOGI("pump", "Initializing pump on SCL GPIO %d, SDA GPIO %d", settings->pump_scl_gpio, settings->pump_sda_gpio);
    pump_context_t *pump_ctx = malloc(sizeof(pump_context_t));
    if (!pump_ctx) {
        PUMP_ERROR_RETURN("Failed to allocate memory for pump context");
    }
    pump_ctx->settings = settings;
    memset(pump_ctx->buf, 0, PUMP_BUFFER_SIZE);

    // Quick and dirty I2C setup to send "FIND" to address 0x67
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = settings->pump_sda_gpio,
        .scl_io_num = settings->pump_scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &pump_ctx->bus_handle);
    if (err != ESP_OK) {
        PUMP_ERROR_RETURN("Failed to create new I2C master bus");
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = settings->pump_i2c_addr,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(pump_ctx->bus_handle, &dev_cfg, &pump_ctx->dev_handle);
    if (err != ESP_OK) {
        PUMP_ERROR_RETURN("Failed to add I2C device to bus");
    }
    return pump_ctx;
}

char* pump_send_cmd(pump_context_t *pump_ctx, const char *cmd) {
    esp_err_t err = i2c_master_transmit(pump_ctx->dev_handle, (uint8_t*)cmd, strlen(cmd), -1);
    if (err != ESP_OK) {
        ESP_LOGE("pump", "Failed to send `%s` command to pump: %s", cmd, esp_err_to_name(err));
        return NULL;
    }

    for (int attempt = 0; attempt < PUMP_MAX_ATTEMPTS; attempt++) {
        memset(pump_ctx->buf, 0, PUMP_BUFFER_SIZE);
        vTaskDelay(pdMS_TO_TICKS(PUMP_PROCESSING_DELAY)); // Wait for pump to process command
        err = i2c_master_receive(pump_ctx->dev_handle, (uint8_t*)pump_ctx->buf, PUMP_BUFFER_SIZE - 1, PUMP_PROCESSING_DELAY);
        switch (err) {
            case ESP_ERR_TIMEOUT:
                ESP_LOGW("pump", "Timeout while waiting for pump response, attempt %d", attempt + 1);
                continue;
            case ESP_OK:
                break;
            default:
                ESP_LOGE("pump", "Error receiving pump response: %s", esp_err_to_name(err));
                return NULL;
        }
        switch (pump_ctx->buf[0]) {
            case 1:
                return pump_ctx->buf+1;
            case 2:
                return NULL; // syntax error
            case 254:
                continue; // still processing; try again
            case 255:
                return ""; // no data
            default:
                ESP_LOGE("pump", "Pump returned unexpected error code: %d", pump_ctx->buf[0]);
                break;
        }
    }
    // No response after max attempts
    ESP_LOGE("pump", "No response from pump after %d attempts", PUMP_MAX_ATTEMPTS);
    return NULL;
}