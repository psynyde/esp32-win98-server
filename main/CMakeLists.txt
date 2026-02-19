idf_component_register(
    SRCS "main.c" "core.c" "server.c"
    INCLUDE_DIRS "."
    EMBED_FILES
        "dist/w95fa.woff2"
        "dist/netscape.gif"
        "dist/built_with_nix.gif"
        "dist/neovim.png"
        "dist/ublockorigin.png"
        "dist/about.html"
        "dist/sysinfo.html"
        "dist/guestbook.html"
    REQUIRES
        esp_wifi
        esp_netif
        nvs_flash
        esp_http_server
        esp_partition
        esp_timer
        vfs
        json
        driver
    PRIV_REQUIRES
        spi_flash
        littlefs
)

