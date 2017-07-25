/*
 * Advanced OLED menu for Huawei E5372 portable LTE router.
 * 
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -s -o oled_hijack.so oled_hijack_so.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>

#define PAGE_INFORMATION 1
#define SUBSYSTEM_GPIO 21002
#define BUTTON_POWER 8
#define BUTTON_MENU 9

/* 
 * Variables from "oled" binary.
 * 
 * g_current_page is a current menu page. -1 for main screen, 0 for main
 * menu screen, 1 for information page.
 * 
 * g_current_Info_page is a pointer to current visible page in the
 * information screen.
 * 
 * Current values are based on E5372 oled binary.
 * MD5: eb4e65509e16c2023f4f9a5e00cd0785
 * 
 */
static uint32_t *g_current_page = (uint32_t*)(0x00029f94);
static uint32_t *g_current_Info_page = (uint32_t*)(0x0002CAB8);

static uint32_t first_info_screen = 0;
static int current_infopage_item = 0;

/*
 * Real handlers from oled binary and libraries
 */
static int (*register_notify_handler_real)(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) = NULL;
static int (*notify_handler_async_real)(int subsystemid, int action, int subaction) = NULL;

/* 
 * Menu-related configuration
 */

static const char *scripts[] = {
    "/app/bin/oled_hijack/radio_mode.sh",
    "/app/bin/oled_hijack/ttlfix.sh",
    "/app/bin/oled_hijack/imei_change.sh",
    NULL
};

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
    "Enabled, TTL=64",
    // 2
    "Enabled, TTL=128",
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

static const char *enabled_disabled_mapping[] = {
    // 0
    "Disabled",
    // 1
    "Enabled",
    NULL
};

struct menu_s {
    uint8_t radio_mode;
    uint8_t ttlfix;
    uint8_t imei_change;
} menu_state;

/* *************************************** */


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
 * Call every script in scripts array and update
 * menu_state struct.
 * Called in sprintf.
 */
static void update_menu_state() {
    int i, ret;

    for (i = 0; scripts[i] != NULL; i++) {
        ret = call_script(scripts[i], "get");
        switch (i) {
            case 0:
                menu_state.radio_mode = ret;
                break;
            case 1:
                menu_state.ttlfix = ret;
                break;
            case 2:
                menu_state.imei_change = ret;
                break;
        }
    }
}

/*
 * Hijacked page button handler.
 * Executes corresponding script with "set_next" argument.
 * Called when the POWER button is pressed on hijacked page.
 */
static void handle_menu_state_change(int menu_page) {
    call_script(scripts[menu_page], "set_next");
}

/*
 * Create menu of 3 items.
 * Only for E5372 display.
 */
static void create_menu_item(char *buf, const char *mapping[], int current_item) {
    int i, char_list_size = 0;
    char nothing[2] = "";

    for (i = 0; mapping[i] != NULL; i++) {
        char_list_size++;
    }

    fprintf(stderr, "Trying to create menu\n");

    if (current_item == 0) {
        snprintf(buf, 1024 - 1,
             "  > %s\n    %s\n    %s\n",
             (mapping[current_item]),
             ((char_list_size >= 2) ? mapping[current_item + 1] : nothing),
             ((char_list_size >= 3) ? mapping[current_item + 2] : nothing)
        );
    }
    else if (current_item == char_list_size - 1) {
        snprintf(buf, 1024 - 1,
             "    %s\n    %s\n  > %s\n",
             ((current_item >= 2 && char_list_size > 2) ? mapping[current_item - 2] : nothing),
             ((current_item >= 1 && char_list_size > 1) ? mapping[current_item - 1] : nothing),
             (mapping[current_item])
        );
    }
    else {
        snprintf(buf, 1024 - 1,
            "    %s\n  > %s\n    %s\n",
            ((current_item > 0) ? mapping[current_item - 1] : nothing),
            (mapping[current_item]),
            ((current_item < char_list_size) ? mapping[current_item + 1] : nothing)
        );
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
    
    if (*g_current_page == PAGE_INFORMATION) {
        if (first_info_screen && first_info_screen != *g_current_Info_page) {
            fprintf(stderr, "We're not on a main info screen!\n");
            if (action == BUTTON_POWER) {
                // button pressed
                fprintf(stderr, "BUTTON PRESSED!\n");
                handle_menu_state_change(current_infopage_item);
                leave_and_enter_menu(current_infopage_item);
                return notify_handler_async_real(subsystemid, BUTTON_MENU, subaction);
            }
            else if (action == BUTTON_MENU) {
                current_infopage_item++;
            }
        }
        else {
            current_infopage_item = 0;
        }
    }

    return notify_handler_async_real(subsystemid, action, subaction);
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
    int i = 0;
    char network_mode_buf[1024];
    char ttlfix_buf[1024];
    char imei_change_buf[1024];

    va_list args;
    va_start(args, format);
    i = vsprintf(str, format, args);
    va_end(args);
    
    if (format && (strcmp(format, "SSID: %s\n") == 0 ||
        strncmp(str, "SSID0: ", 7) == 0 ||
        strcmp(format, "PWD: %s\n") == 0 ||
        strncmp(str, "PWD0: ", 6) == 0)) {
            va_start(args, format);
            i = vsnprintf(str, 20, format, args);
            str[19] = '\0';
            va_end(args);
    }
    else if (format && (strncmp(str, "SSID1: ", 7) == 0 ||
        strncmp(str, "PWD1: ", 6) == 0)) {
            i = snprintf(str, 2, "");
    }

    // Hijacking "Homepage: %s" string on second information page
    if (format && strcmp(format, "Homepage: %s") == 0) {
        fprintf(stderr, "FOUND STRING!\n");
        update_menu_state();
        create_menu_item(network_mode_buf, network_mode_mapping, menu_state.radio_mode);
        create_menu_item(ttlfix_buf, ttlfix_mapping, menu_state.ttlfix);
        create_menu_item(imei_change_buf, imei_change_mapping, menu_state.imei_change);
        snprintf(str, 999,
                 "# Network Mode:\n%s" \
                 "# TTL Mangling:\n%s" \
                 "# Device IMEI:\n%s",
                 network_mode_buf,
                 ttlfix_buf,
                 imei_change_buf
        );
        //fprintf(stderr, "%s\n",);
    }
    fprintf(stderr, "sprintf %s\n", format);
    return i;
}

int osa_print_log_ex(char *subsystem, char *sourcefile, int line,
                     int offset, const char *message, ...) {
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
