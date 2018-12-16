/*
 * Advanced OLED menu for Huawei E5372 portable LTE router.
 * 
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -s \
 * -D__ANDROID_API__=9 -DMENU_UNLOCK -o oled_hijack.so oled_hijack_so.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAGE_INFORMATION 1
#define SUBSYSTEM_GPIO 21002
#define EVT_OLED_WIFI_WAKEUP 14026
#define EVT_DIALUP_REPORT_CONNECT_STATE 4037
#define EVT_OLED_WIFI_STA_CHANGE 14010
#define DIAL_STATE_CONNECTING 900
#define DIAL_STATE_CONNECTED 901
#define DIAL_STATE_DISCONNECTED 902
#define BUTTON_POWER 8
#define BUTTON_MENU 9
#define LED_ON 0

/* 
 * Variables from "oled" binary.
 * 
 * g_current_page is a current menu page. -1 for main screen, 0 for main
 * menu screen, 1 for information page.
 * 
 * g_current_Info_page is a pointer to current visible page on the
 * information screen.
 * 
 * Current values are based on E5372 21.290.23.00.00 oled binary.
 * MD5:  eb4e65509e16c2023f4f9a5e00cd0785
 * SHA1: a68f381df9cbeccd242e16fc790f92741f3e049e
 * 
 */
static uint32_t volatile *g_current_page = (uint32_t volatile *)(0x00029f94);
static uint32_t volatile *g_current_Info_page = (uint32_t volatile *)(0x0002CAB8);
static uint32_t volatile *g_led_status = (uint32_t volatile *)(0x00029FA8);

static uint32_t first_info_screen = 0;

// Do not send BUTTON_PRESSED event notify to "oled" if set.
// Something like a mutex, but we don't really care about
// thread-safety as we only care of atomic writes which are
// in the same thread.
static int lock_buttons = 0;

static int current_infopage_item = 0;
static int custom_script_enabled = -1;

/*
 * Real handlers from oled binary and libraries
 */
static int (*register_notify_handler_real)(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) = NULL;
static int (*notify_handler_async_real)(int subsystemid, int action, int subaction) = NULL;

/* 
 * Menu-related configuration
 */

static const char *network_mode_mapping[] = {
    // 0
    "Auto",
    // 1
    "GSM Only",
    "UMTS Only",
    "LTE Only",
    "LTE -> UMTS",
    "LTE -> GSM",
    // 6
    "UMTS -> GSM",
    NULL
};

static const char *ttlfix_mapping[] = {
    // 0
    "Disabled",
    // 1
    "TTL=64",
    // 2
    "TTL=128",
    // 3
    "TTL=65 (WiFi Ext.)",
    NULL
};

static const char *dns_over_tls_mapping[] = {
    // 0
    "Disabled",
    // 1
    "Enabled",
    // 2
    "Enabled + adblock",
    NULL
};

static const char *imei_change_mapping[] = {
    // 0
    "Stock",
    // 1
    "Random Android",
    // 2
    "Random WinPhone",
    NULL
};

static const char *remote_access_mapping[] = {
    // 0
    "Web only",
    // 1
    "Web & Telnet",
    // 2
    "Web, Telnet, ADB",
    // 3
    "Telnet & ADB only",
    // 4
    "All disabled",
    NULL
};

static const char *usb_mode_mapping[] = {
    // 0
    "Stock",
    // 1
    "AT, Network, SD",
    // 2
    "AT, Network",
    // 3
    "Debug mode",
    NULL
};

static const char *enabled_disabled_mapping[] = {
    // 0
    "Disabled",
    // 1
    "Enabled",
    NULL
};

#define OLED_CUSTOM "/online/oled_custom.sh"
#define SCRIPT_PATH "/app/bin/oled_hijack"
#define NET_DOWN_SCRIPT SCRIPT_PATH "/net.down"
#define NET_UP_SCRIPT   SCRIPT_PATH "/net.up"

struct script_s {
    const char *title;
    const char *path;
    const char **mapping;
    uint8_t slow_script; // no support for slow_script yet for 128x128 version
    int state;
};

static struct script_s scripts[] = {
    {"# Network Mode:", SCRIPT_PATH "/radio_mode.sh", network_mode_mapping, 1, 0},
    {"# IMEI (req. reboot):", SCRIPT_PATH "/imei_change.sh", imei_change_mapping, 1, 0},
    {"# TTL (req. reboot):", SCRIPT_PATH "/ttlfix.sh", ttlfix_mapping, 0, 0},
    {"# Anticensorship:", SCRIPT_PATH "/anticensorship.sh", enabled_disabled_mapping, 0, 0},
    {"# DNS over TLS:", SCRIPT_PATH "/dns_over_tls.sh", dns_over_tls_mapping, 1, 0},
    {"# Remote Access:", SCRIPT_PATH "/remote_access.sh", remote_access_mapping, 0, 0},
    {"# Work w/o Battery:", SCRIPT_PATH "/no_battery.sh", enabled_disabled_mapping, 0, 0},
    {"# USB Mode:", SCRIPT_PATH "/usb_mode.sh", usb_mode_mapping, 0, 0},
    //{"# Wi-Fi (w/reboot):", SCRIPT_PATH "/wifi.sh", enabled_disabled_mapping, 1, 0},
    /* Is it assumed that Custom Script is always the latest item, don't remove it. */
    {"# Custom Script:", OLED_CUSTOM, enabled_disabled_mapping, 1, 0}
};

// Number of scripts in the array above. Filled in runtime.
static int scripts_count = 0;
#ifdef MENU_UNLOCK
// Advanced menu unlock flag.
// The user should press POWER button 7 times on information page
// to activate the menu.
static int menu_unlocked = 0;
#define UNLOCK_PRESS_COUNT 7
#endif

/* *************************************** */

#define LOCKBUTTONS(x) (x ? (lock_buttons = 1) : (lock_buttons = 0))

/* 
 * Execute shell script and return exit code.
 */
static int call_script(char const *script, char const *additional_argument) {
    int ret;
    char arg_buff[1024];

    if (additional_argument) {
        snprintf(arg_buff, sizeof(arg_buff) - 1, "%s %s",
                 script, additional_argument);
    }
    else {
        snprintf(arg_buff, sizeof(arg_buff) - 1, "%s",
                 script);
    }

    fprintf(stderr, "Calling script: %s\n", arg_buff);
    ret = system(arg_buff);
    if (WIFSIGNALED(ret) &&
        (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
        return -1;

    fprintf(stderr, "GOT RET: %d\n", WEXITSTATUS(ret));
    return WEXITSTATUS(ret);
}

/*
 * Call all scripts in scripts array and update their state.
 * Called in sprintf.
 */
static void update_menu_state() {
    int menu_page = 0;

    for (menu_page = 0; menu_page < scripts_count; menu_page++) {
        scripts[menu_page].state = call_script(scripts[menu_page].path, "get");
    }
}

/*
 * Hijacked page button handler.
 * Executes corresponding script with "set_next" argument.
 * Called when the POWER button is pressed on hijacked page.
 */
static void handle_menu_state_change(int menu_page) {
    call_script(scripts[menu_page].path, "set_next");
}

/*
 * Create menu of 3 items.
 * Only for 128x128 displays.
 */
static void create_menu_item(char *buf, size_t bufsize, const char *mapping[],
                             int current_item) {
    int i, char_list_size = 0;
    static const char nothing[] = "";

    for (i = 0; mapping[i] != NULL; i++) {
        char_list_size++;
    }

    fprintf(stderr, "Trying to create menu\n");

    if (current_item == 0) {
        snprintf(buf, bufsize,
             "  > %s\n    %s\n    %s\n",
             (mapping[current_item]),
             ((char_list_size >= 2) ? mapping[current_item + 1] : nothing),
             ((char_list_size >= 3) ? mapping[current_item + 2] : nothing)
        );
    }
    else if (current_item == char_list_size - 1 && char_list_size >= 3) {
        snprintf(buf, bufsize,
             "    %s\n    %s\n  > %s\n",
             ((current_item >= 2 && char_list_size > 2) ? mapping[current_item - 2] : nothing),
             ((current_item >= 1 && char_list_size > 1) ? mapping[current_item - 1] : nothing),
             (mapping[current_item])
        );
    }
    else if (current_item <= char_list_size) {
        snprintf(buf, bufsize,
            "    %s\n  > %s\n    %s\n",
            ((current_item > 0) ? mapping[current_item - 1] : nothing),
            (mapping[current_item]),
            ((current_item < char_list_size && mapping[current_item + 1]) \
                                    ? mapping[current_item + 1] : nothing)
        );
    }
    else {
        snprintf(buf, bufsize,
            "    ERROR\n\n\n");
    }
}

/* 
 * Function which presses buttons to leave information page
 * and enter it again, to force redraw.
 * Very dirty, but works.
 * 
 * Assuming information page is a first menu item.
 * 
 */
static void leave_and_enter_menu(int advance) {
    int i;

    *g_current_Info_page = 0;
    // selecting "back"
    notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_MENU, 0);
    // pressing "back"
    notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_POWER, 0);
    // selecting "device information"
    notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_MENU, 0);
    // pressing "device information"
    notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_POWER, 0);

    // advancing to the exact page we were on
    for (i = 0; i <= advance; i++) {
        notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_MENU, 0);
    }
}

static int notify_handler_async(int subsystemid, int action, int subaction) {
    fprintf(stderr, "notify_handler_async: %d, %d, %x\n", subsystemid, action, subaction);

    if (subsystemid == EVT_OLED_WIFI_WAKEUP) {
        // Do NOT notify "oled" of EVT_OLED_WIFI_WAKEUP event.
        // Fixes "exiting sleep mode" on every button
        // if Wi-Fi is completely disabled in web interface.
        return 0;
    }

    else if (*g_current_page == PAGE_INFORMATION &&
        subsystemid == EVT_DIALUP_REPORT_CONNECT_STATE &&
        action == DIAL_STATE_CONNECTING) {
        // Do NOT notify "oled" of EVT_DIALUP_REPORT_CONNECT_STATE
        // with action=DIAL_STATE_CONNECTING while on info page.
        // We do not want to draw animations in the middle of network
        // change from the menu.
        return 0;
    }

    else if (subsystemid == EVT_OLED_WIFI_STA_CHANGE ||
        subsystemid == EVT_DIALUP_REPORT_CONNECT_STATE)
    {
        // Call net.up/net.down scripts when network state
        // changes or when mobile network switches to Wi-Fi repeater
        // or vice versa.
        if (action == DIAL_STATE_DISCONNECTED)
            call_script(NET_DOWN_SCRIPT, NULL);
        else if (action == DIAL_STATE_CONNECTED)
            call_script(NET_UP_SCRIPT, NULL);
    }

    if (*g_current_page == PAGE_INFORMATION) {
#ifdef MENU_UNLOCK
        if (first_info_screen && first_info_screen == *g_current_Info_page) {
            if (subsystemid == SUBSYSTEM_GPIO &&
                *g_led_status == LED_ON &&
                menu_unlocked != UNLOCK_PRESS_COUNT)
            {
                if (action == BUTTON_POWER) {
                    // POWER button pressed on information page,
                    // increasing menu_unlocked
                    menu_unlocked++;
                    if (menu_unlocked == UNLOCK_PRESS_COUNT) {
                        leave_and_enter_menu(0);
                        return 0;
                    }
                }
                else if (action == BUTTON_MENU)
                    menu_unlocked = 0;
            }
        }
#endif

        if (first_info_screen && first_info_screen != *g_current_Info_page) {
            if (subsystemid == SUBSYSTEM_GPIO && *g_led_status == LED_ON) {
                fprintf(stderr, "We're not on a main info screen!\n");
                if (lock_buttons) {
                    // Do NOT notify "oled" of button events
                    // if buttons are locked by slow script
                    return 0;
                }
                if (action == BUTTON_POWER) {
                    // button pressed
                    fprintf(stderr, "BUTTON PRESSED!\n");
                    // lock buttons to prevent user intervention
                    LOCKBUTTONS(1);
                    handle_menu_state_change(current_infopage_item);
                    leave_and_enter_menu(current_infopage_item);
                    LOCKBUTTONS(0);
                    return notify_handler_async_real(subsystemid, BUTTON_MENU, subaction);
                }
                else if (action == BUTTON_MENU) {
                    current_infopage_item++;
                }
            }
        }
        else {
            current_infopage_item = 0;
        }
    }

    return notify_handler_async_real(subsystemid, action, subaction);
}


static void create_and_write_menu(char *outbuf, size_t outbuf_size) {
    char tempbuf[1024] = {0};
    char finalbuf[1024] = {0};
    int menu_item = 0;

    for (menu_item = 0; menu_item < scripts_count; menu_item++) {
        create_menu_item(tempbuf,
                         sizeof(tempbuf),
                         scripts[menu_item].mapping,
                         scripts[menu_item].state);
        snprintf(finalbuf, sizeof(finalbuf), "%s%s\n%s", finalbuf,
                 scripts[menu_item].title, tempbuf);
    }
    snprintf(outbuf, outbuf_size, "%s", finalbuf);
}

/*
 * Hijacked functions from various libraries.
 */

int register_notify_handler(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) {
    if (!register_notify_handler_real) {
        register_notify_handler_real = dlsym(RTLD_NEXT, "register_notify_handler");
    }
    //fprintf(stderr, "register_notify_handler: %d, %d, %d\n", a1, a2, a3);
    notify_handler_async_real = notify_handler_async_orig;
    return register_notify_handler_real(subsystemid, notify_handler_sync, notify_handler_async);
}

int sprintf(char *str, const char *format, ...) {
    int i = 0, j = 0;

    va_list args;
    va_start(args, format);
    i = vsprintf(str, format, args);
    va_end(args);

    if (format && (strcmp(format, "SSID: %s\n") == 0 ||
        strncmp(str, "SSID0: ", 7) == 0 ||
        strcmp(format, "PWD: %s\n") == 0 ||
        strncmp(str, "PWD0: ", 6) == 0))
    {
        // Cut SSID or password to prevent line break
        va_start(args, format);
        i = vsnprintf(str, 19, format, args);
        str[18] = ' ';
        str[19] = '\0';
        va_end(args);
    }
    else if (format && (strncmp(str, "SSID1: ", 7) == 0 ||
        strncmp(str, "PWD1: ", 6) == 0))
    {
        // Remove multi-ssid information to keep everything
        // on a single screen.
        i = snprintf(str, 2, "");
    }

    // Hijacking "Homepage: %s" string on second information page
    if (format && strcmp(format, "Homepage: %s") == 0) {
        // Update scripts_count variable if it's zero.
        if (!scripts_count) {
            scripts_count = sizeof(scripts) / sizeof(struct script_s);
            if (access(OLED_CUSTOM, F_OK) != 0) {
                scripts_count--;
            }
        }
        fprintf(stderr, "FOUND STRING!\n");
#ifdef MENU_UNLOCK
        if (menu_unlocked != UNLOCK_PRESS_COUNT) {
            // Do nothing if menu is not yet unlocked.
            strcpy(str, "");
            return 0;
        }
#endif
        update_menu_state();
        create_and_write_menu(str, 999);
        //fprintf(stderr, "%s\n",);
    }
    fprintf(stderr, "sprintf %s\n", format);
    return i;
}

int osa_print_log_ex(char *subsystem, char *sourcefile, int line,
                     int offset, const char *message, ...) {
    // Uncomment to watch debug prints from oled binary.
    /*va_list args;
    va_start(args, message);
    fprintf(stderr, "[%s] %s (%d): ", subsystem, sourcefile, line);
    vprintf(message, args);
    va_end(args);*/

    if (*g_current_page == PAGE_INFORMATION) {
        if (!first_info_screen) {
            first_info_screen = *g_current_Info_page;
            fprintf(stderr, "Saved first screen address!\n");
        }
    }
    else {
        first_info_screen = 0;
    }

    return 0;
}
