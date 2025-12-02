#include "metrics.h"
#include "weight.h"
#include "wifi.h"
#include "bthome_observer.h"
#include "bthome.h"
#include "IQmathLib.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "metrics";

// Context for BTHome metrics iteration
typedef struct {
    char *buffer;
    size_t buffer_size;
    int *offset;
    settings_t *settings;
    const char *hostname;
} bthome_metrics_ctx_t;

// Helper function to check if an object ID is selected
static bool is_object_id_selected(uint8_t object_id, settings_t *settings) {
    if (settings->selected_bthome_object_ids == NULL || 
        settings->selected_bthome_object_ids_count == 0) {
        return false;
    }
    
    for (size_t i = 0; i < settings->selected_bthome_object_ids_count; i++) {
        if (settings->selected_bthome_object_ids[i] == object_id) {
            return true;
        }
    }
    
    return false;
}

// Callback for iterating BTHome cached packets
static bool bthome_metrics_iterator(const esp_bd_addr_t addr, int rssi, 
                                     const bthome_packet_t *packet, void *user_data) {
    bthome_metrics_ctx_t *ctx = (bthome_metrics_ctx_t *)user_data;
    char mac_str[18];
    
    // Format MAC address
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    // Add RSSI metric for this device
    *ctx->offset += snprintf(ctx->buffer + *ctx->offset, 
                             ctx->buffer_size - *ctx->offset,
                             "bthome_rssi_dbm{hostname=\"%s\",mac=\"%s\"} %d\n",
                             ctx->hostname, mac_str, rssi);
    
    // Iterate through measurements
    for (size_t i = 0; i < packet->measurement_count; i++) {
        const bthome_measurement_t *m = &packet->measurements[i];
        
        // Check if this object ID is selected in settings
        if (!is_object_id_selected(m->object_id, ctx->settings)) {
            continue;
        }
        
        float factor = bthome_get_scaling_factor(m->object_id);
        float value = bthome_get_scaled_value(m, factor);
        const char *name = bthome_get_object_name(m->object_id);
        
        if (name == NULL) {
            continue;
        }
        
        // Convert name to Prometheus-friendly metric name (lowercase, underscores)
        char metric_name[128];
        snprintf(metric_name, sizeof(metric_name), "bthome_%s", name);
        
        // Replace spaces and hyphens with underscores, convert to lowercase
        for (char *p = metric_name; *p; p++) {
            if (*p == ' ' || *p == '-') {
                *p = '_';
            } else if (*p >= 'A' && *p <= 'Z') {
                *p = *p + ('a' - 'A');
            }
        }
        
        // Add the metric
        *ctx->offset += snprintf(ctx->buffer + *ctx->offset, 
                                 ctx->buffer_size - *ctx->offset,
                                 "%s{hostname=\"%s\",mac=\"%s\"} %.2f\n",
                                 metric_name, ctx->hostname, mac_str, value);
    }
    
    return true;  // Continue iteration
}


static esp_err_t metrics_handler(httpd_req_t *req) {
    settings_t *settings = (settings_t *)req->user_ctx;
    
    // Allocate larger buffer for BTHome metrics
    size_t response_size = 8192;
    char *response = malloc(response_size);
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for metrics response");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    int offset = 0;
    
    // Get uptime in seconds
    int64_t uptime_us = esp_timer_get_time();
    int64_t uptime_seconds = uptime_us / 1000000;
    
    // Get weight
    bool weight_available = false;
    float weight = weight_get_latest(&weight_available);
    int32_t weight_raw = weight_get_latest_raw(&weight_available);
    
    // Get WiFi RSSI
    int8_t rssi = wifi_get_rssi();
    
    // Get hostname for labels
    const char *hostname = (settings->hostname != NULL && settings->hostname[0] != '\0') 
                            ? settings->hostname : "weight-station";
    
    // Build Prometheus text format response
    // Weight metric
    offset += snprintf(response + offset, response_size - offset,
                      "# HELP weight_grams Current weight reading in grams\n"
                      "# TYPE weight_grams gauge\n");
    
    if (weight_available) {
        offset += snprintf(response + offset, response_size - offset,
                          "weight_grams{hostname=\"%s\"} %.2f\n", hostname, weight);
    }

    // Build Prometheus text format response
    // Raw weight metric
    offset += snprintf(response + offset, response_size - offset,
                      "# HELP weight_raw Current weight reading in raw units\n"
                      "# TYPE weight_raw gauge\n");
    
    if (weight_available) {
        offset += snprintf(response + offset, response_size - offset,
                          "weight_raw{hostname=\"%s\"} %" PRIi32 "\n", hostname, weight_raw);
    }
    
    // WiFi RSSI metric
    offset += snprintf(response + offset, response_size - offset,
                      "# HELP wifi_rssi_dbm WiFi signal strength in dBm\n"
                      "# TYPE wifi_rssi_dbm gauge\n");
    
    if (rssi != 0) {
        offset += snprintf(response + offset, response_size - offset,
                          "wifi_rssi_dbm{hostname=\"%s\"} %d\n", hostname, rssi);
    }
    
    // Uptime metric
    offset += snprintf(response + offset, response_size - offset,
                      "# HELP uptime_seconds System uptime in seconds\n"
                      "# TYPE uptime_seconds counter\n"
                      "uptime_seconds{hostname=\"%s\"} %lld\n", hostname, uptime_seconds);
    
    // BTHome RSSI metric header
    offset += snprintf(response + offset, response_size - offset,
                      "# HELP bthome_rssi_dbm BTHome device signal strength in dBm\n"
                      "# TYPE bthome_rssi_dbm gauge\n");
    
    // Add BTHome metrics from cache
    if (settings->selected_bthome_object_ids_count > 0) {
        bthome_metrics_ctx_t ctx = {
            .buffer = response,
            .buffer_size = response_size,
            .offset = &offset,
            .settings = settings,
            .hostname = hostname
        };
        
        bthome_cache_iterate(bthome_metrics_iterator, &ctx);
    }
    
    // Set response headers and send
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, "text/plain; version=0.0.4");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_send(req, response, offset);
    
    free(response);
    return ESP_OK;
}

static httpd_uri_t metrics_uri = {
    .uri       = "/metrics",
    .method    = HTTP_GET,
    .handler   = metrics_handler,
    .user_ctx  = NULL
};

void metrics_init(settings_t *settings, httpd_handle_t server) {
    metrics_uri.user_ctx = settings;
    esp_err_t err = httpd_register_uri_handler(server, &metrics_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) registering metrics handler!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Prometheus metrics endpoint registered at /metrics");
    }
}
