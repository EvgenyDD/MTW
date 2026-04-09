set(include_dirs . console led web drive)

set(srcs 
    "main.c"
    "console_cb.c"
    "console/console.c"
    "console/console_cb_utils.c"
    "drive/drive.c"
    "led/led_drv.c"
    "led/led_drv_samples.c"
    "debounce.c"
    "sh1107.c"
    "ui.c"
    "web/dns_server.c"
    "web/time_wrapper.c"
    "web/web_common.c"
    "web/ws.c"
    "web/wifi.c")

FILE(GLOB fonts fonts/*.c)

idf_component_register(
    SRCS "${srcs}" "${fonts}"
    INCLUDE_DIRS "${include_dirs}"
    EMBED_FILES web/page/dist/index.html)