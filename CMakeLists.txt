set(include_dirs "include/")
set(src_dirs "src/")

set(requires
    esp_http_server
    log
)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)
