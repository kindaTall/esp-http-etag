/**
 * @file esp_http_etag.c
 * @brief HTTP server utilities for serving compressed files with ETags
 *
 * Implements efficient serving of gzip-compressed embedded files with
 * ETag-based caching to reduce bandwidth usage.
 */
#include "esp_http_etag.h"

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "esp_http_etag";

// ============================================================================
// BINARY FILE SERVING WITH ETAG CACHING
// ============================================================================

/**
 * @brief Send binary data as HTTP response with gzip encoding and ETag caching
 *
 * Checks If-None-Match header and sends 304 Not Modified if content hasn't changed.
 * Otherwise, sends full response with gzip encoding and ETag header.
 *
 * @param req       HTTP request handle
 * @param start     Pointer to start of binary data (gzip-compressed)
 * @param end       Pointer to end of binary data
 * @param type      MIME type (e.g., "text/html", "application/javascript")
 * @param etag      ETag string for caching (NULL to disable caching)
 * @param chunked   Use chunked transfer encoding (0 for single send)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t send_binaries(httpd_req_t *req, const char *start, const char *end,
                        const char *type, const char *etag, uint8_t chunked)
{
    esp_err_t err;

    // Validate pointers (defensive)
    if (!req || !start || !end || !type || end < start) {
        ESP_LOGE(TAG, "Invalid arguments to send_binaries");
        return ESP_ERR_INVALID_ARG;
    }

    // Set response headers
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    // ETag caching: Check if client has cached version
    if (etag != NULL && strlen(etag) > 0 && strlen(etag) <= ETAG_MAX_LENGTH) {
        char if_none_match[ETAG_MAX_LENGTH+1] = {0}; // Zero-init buffer
        if (httpd_req_get_hdr_value_str(req, "If-None-Match", if_none_match, sizeof(if_none_match)) == ESP_OK) {
            // Ensure null termination (defensive)
            if_none_match[ETAG_MAX_LENGTH] = '\0';

            if (strcmp(if_none_match, etag) == 0) {
                // Content hasn't changed, send 304 Not Modified
                ESP_LOGI(TAG, "Sending 304 Not Modified for %s", type);
                httpd_resp_set_status(req, "304 Not Modified");
                httpd_resp_send(req, NULL, 0);
                return ESP_OK;
            }
        }

        // Content has changed or no cached version, set ETag header
        httpd_resp_set_hdr(req, "ETag", etag);
    }

    // Send the actual content
    size_t content_len = end - start;
    if (chunked) {
        err = httpd_resp_send_chunk(req, start, content_len);
    } else {
        err = httpd_resp_send(req, start, content_len);
    }

    ESP_LOGD(TAG, "Sent %s (%zu bytes, chunked: %d)", type, content_len, chunked);
    return err;
}

// ============================================================================
// URL QUERY PARAMETER PARSING
// ============================================================================

/**
 * @brief Parse integer value from URL query parameter
 *
 * Extracts a query parameter and converts it to uint64_t.
 * Example: "?param1=123&param2=456" with key="param1" returns 123
 *
 * @param qry   Query string (e.g., "param1=123&param2=456")
 * @param key   Parameter key to search for
 * @param val   Output pointer for parsed integer value
 * @return esp_err_t
 *   - ESP_OK: Key found and parsed successfully
 *   - ESP_ERR_NOT_FOUND: Key not found
 *   - ESP_ERR_INVALID_ARG: Null arguments or non-numeric characters
 */
esp_err_t httpd_query_key_integer(const char *qry, const char *key, uint64_t *val)
{
    esp_err_t err;
    char buff[64];  // NOT static! Each call needs its own buffer

    // Extract the key's value as a string
    err = httpd_query_key_value(qry, key, buff, sizeof(buff));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Couldn't find '%s' in query '%s' (%s)", key, qry, esp_err_to_name(err));
        return err;
    }

    // Convert string to integer (simple decimal parsing)
    uint64_t res = 0;
    char *buff_ptr = buff;
    while (*buff_ptr != '\0') {
        if (*buff_ptr < '0' || *buff_ptr > '9') {
            ESP_LOGW(TAG, "Non-numeric character in value for key '%s': %s", key, buff);
            return ESP_ERR_INVALID_ARG;
        }
        res = (res * 10) + (*buff_ptr - '0');
        buff_ptr++;
    }
    *val = res;

    ESP_LOGD(TAG, "Parsed query key '%s' = %"PRIu64, key, res);
    return ESP_OK;
}
