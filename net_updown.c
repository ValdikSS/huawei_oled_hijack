/*
 * Network up/down hook for Huawei portable LTE routers.
 * This hijack library calls net.down/net.up scripts on network
 * reconfiguration.
 * It reimplements oled_hijack functionality and should be used
 * only on devices without OLED screen, where oled_hijack could not be used.
 *
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -D__ANDROID_API__=19 -s -o net_updown.so net_updown.c
 * Inject it into 'led' binary.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#define EVT_DIALUP_REPORT_CONNECT_STATE 4037
#define EVT_OLED_WIFI_STA_CHANGE 14010
#define DIAL_STATE_CONNECTED 901
#define DIAL_STATE_DISCONNECTED 902

#define SCRIPT_PATH "/app/bin/led_hijack"

#define NET_DOWN_SCRIPT SCRIPT_PATH "/net.down"
#define NET_UP_SCRIPT   SCRIPT_PATH "/net.up"

/*
 * Real handlers from the binary and libraries
 */
static int (*register_notify_handler_real)(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) = NULL;
static int (*notify_handler_async_real)(int subsystemid, int action, int subaction) = NULL;

static int notify_handler_async(int subsystemid, int action, int subaction) {
    //fprintf(stderr, "notify_handler_async: %d, %d, %d\n", subsystemid, action, subaction);
    if (subsystemid == EVT_OLED_WIFI_STA_CHANGE ||
        subsystemid == EVT_DIALUP_REPORT_CONNECT_STATE)
    {
        // Call net.up/net.down scripts when network state
        // changes or when mobile network switches to Wi-Fi repeater
        // or vice versa.
        if (action == DIAL_STATE_DISCONNECTED)
            system(NET_DOWN_SCRIPT);
        else if (action == DIAL_STATE_CONNECTED)
            system(NET_UP_SCRIPT);
    }

    return notify_handler_async_real(subsystemid, action, subaction);
}

/*
 * Hijacked function.
 */

int register_notify_handler(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) {
    unsetenv("LD_PRELOAD");
    if (!register_notify_handler_real) {
        register_notify_handler_real = dlsym(RTLD_NEXT, "register_notify_handler");
    }
    //fprintf(stderr, "register_notify_handler: %d, %d, %d\n", subsystemid, notify_handler_sync, notify_handler_async_orig);
    notify_handler_async_real = notify_handler_async_orig;
    return register_notify_handler_real(subsystemid, notify_handler_sync, notify_handler_async);
}
