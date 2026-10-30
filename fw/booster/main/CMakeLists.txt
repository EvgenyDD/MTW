set(include_dirs . console led web drive ../../dcc)

set(srcs 
    "main.c"
    "console_cb.c"
    "console/console.c"
    "console/console_cb_utils.c"
    "drive/dcc_fb.c"
    "drive/dcc_hw.c"
    "../../dcc/dcc_decoder.c"
    "../../dcc/dcc_coder.c"
    "drive/hl_irq.S"
    "led/led_drv.c"
    "led/led_drv_samples.c"
    "debounce.c"
    "ina226.c"
    "safety.c"
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

target_link_libraries(${COMPONENT_TARGET} "-u ld_include_xt_highint5")