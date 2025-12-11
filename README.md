# esp_http_etag

ESP-IDF component for serving gzip-compressed embedded web assets from `esp_http_server` with ETag caching.

- Assets are compressed at build time; the device sends them with `Content-Encoding: gzip`.
- The client's `If-None-Match` header is compared with the stored ETag; on a match the response is `304 Not Modified`.
- Depends only on ESP-IDF components (`esp_http_server`, `log`).

## Usage

### 1. Compress assets

```bash
cd esp_http_etag/tools
cp compress_config.json.template compress_config.json   # list your www/ directories
python compress_assets.py [config_file]
```

For each listed file this writes `<file>.gz`, and for each directory an `etags.h` with one `<symbol>_etag` define per file.

`compress_config.json`:

```json
{
  "root": "../../",
  "targets": [
    { "directory": "my_component/www", "files": ["*.html", "*.css", "*.js"] }
  ]
}
```

`root` is optional. Without it, relative directories are searched for upwards from the current directory.

### 2. Embed the compressed files

```cmake
idf_component_register(
    SRC_DIRS "src"
    REQUIRES esp_http_etag
    EMBED_FILES "www/index.html.gz" "www/style.css.gz"
)
```

### 3. Serve them

```c
#include "esp_http_etag.h"
#include "www/etags.h"

DECLARE_EMBEDDED_FILE(index_html_gz);

esp_err_t index_handler(httpd_req_t *req) {
    return SEND_REGISTERED_BINARY(index_html_gz, "text/html");
}
```

`SEND_REGISTERED_BINARY` expects the request variable to be named `req` and the define `index_html_gz_etag` from `etags.h`.

Symbol names follow the ESP-IDF rule for embedded files: every non-alphanumeric character becomes `_`, so `chart.umd.min.js.gz` becomes `chart_umd_min_js_gz`.

## API

```c
esp_err_t send_binaries(httpd_req_t *req, const char *start, const char *end,
                        const char *type, const char *etag, uint8_t chunked);
```

Sends `[start, end)` as a gzip-encoded response of MIME type `type`. If `etag` is not NULL, it answers `304` when the client's ETag matches. With `chunked` set, the data goes out as one chunk via `httpd_resp_send_chunk`, and the caller ends the response. Returns `ESP_ERR_INVALID_ARG` for invalid arguments.

```c
esp_err_t httpd_query_key_integer(const char *qry, const char *key, uint64_t *val);
```

Parses an unsigned integer query parameter. Returns `ESP_ERR_NOT_FOUND` if the key is missing and `ESP_ERR_INVALID_ARG` if the value is not numeric.

Helper macros: `EMBEDDED_FILE_START(f)`, `EMBEDDED_FILE_END(f)`, `EMBEDDED_FILE_SIZE(f)`.

## License

MIT, see [LICENSE](LICENSE).
