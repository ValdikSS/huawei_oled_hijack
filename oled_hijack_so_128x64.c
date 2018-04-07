/*
 * Advanced OLED menu for Huawei E5770/E5885 portable LTE router.
 * 
 * Compile for E5770:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -DE5770 \
 * -D__ANDROID_API__=19 -s -o oled_hijack.so oled_hijack_so_128x64.c
 * Compile for E5885:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -DE5885 \
 * -D__ANDROID_API__=19 -s -o oled_hijack.so oled_hijack_so_128x64.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define PAGE_INFORMATION 8
#define PAGE_BEFORE_INFORMATION 5
#define SUBSYSTEM_GPIO 21002
#define EVT_OLED_WIFI_WAKEUP 14026
#define EVT_DIALUP_REPORT_CONNECT_STATE 4037
#define DIAL_STATE_CONNECTING 900
#define BUTTON_POWER 9
#define BUTTON_MENU 8
#define LED_ON 0

#define OLED_CUSTOM "/online/oled_custom.sh"
#define SCRIPT_PATH "/app/bin/oled_hijack"

/* 
 * Variables from "oled" binary.
 *
 *  g_current_page is a current menu page. 
 *  0 for main screen, 5 for Wi-Fi info, 7 for IP address, 8 for homepage.
 *
 *  g_led_status is a status of LED backlight.
 *  0 for enabled, 1 for timed out but still enabled, 2 for disabled.
 *  g_led_status is usually the latest DWORD in .data segment, and
 *  could be found in functions which use lcd_control_operate and bsp_led_ctrl.
 *
 *  g_main_domain is a char buffer for storing homepage domain, but it is used
 *  as a storage for a pointer to another buffer, which is created by oled_hijack.
 *
 *  g_loaddomain_code is a code which loads first character from g_main_domain
 *  to check if it's NULL. It is patched to load the pointer from first DWORD.
 *
 *  For E5770, current values are based on 21.329.01.00.00 oled binary.
 *  MD5: 6209a2b9560b3a7c2ca26e55f5861fa4
 *
 *  For E5885, current values are based on 21.236.05.01.233 oled binary.
 *  MD5: 96add6d12bc765cbbfed43a88e93e39a
 *
 */

#if !(defined(E5770) || defined(E5885))
#error "You need to define either E5770 or E5885"
#endif

// These values get aligned to real addresses from notify_handler_async
#ifdef E5885
static uint32_t *g_current_page = (uint32_t*)(0x00004438); // end_data + 0x4438. 8 is for homepage.
static uint32_t *g_led_status = (uint32_t*)(0x00002C90);  // start_data + 0x2C90
static uint32_t *g_main_domain = (uint32_t*)(0x0000416C); // end_data + 0x416C, used as dword pointer, not char!!!
static uint16_t *g_loaddomain_code = (uint16_t*)(0x0000DB78); // start_text + 0xDB78, LDRB R0, [R1]
#endif

// for 21.318.01.00.00 oled binary, MD5: 67a52b23d7d2d13ffeca446fcc30eccd. This binary has debug text strings.
/*#ifdef E5770
static uint32_t *g_current_page = (uint32_t*)(0x00003A24); // end_data + 0x3A24. 8 is for homepage.
static uint32_t *g_led_status = (uint32_t*)(0x00002234);  // start_data + 0x2234
static uint32_t *g_main_domain = (uint32_t*)(0x00003804); // end_data + 0x3804, used as dword pointer, not char!!!
static uint16_t *g_loaddomain_code = (uint16_t*)(0x0000B86A); // start_text + 0xB86A, LDRB R0, [R1]
#endif*/

// for 21.329.01.00.00 oled binary. This binary doesn't have debug text strings.
#ifdef E5770
static uint32_t *g_current_page = (uint32_t*)(0x00003A78); // end_data + 0x3A78. 8 is for homepage.
static uint32_t *g_led_status = (uint32_t*)(0x00002298);  // start_data + 0x002298 (or BSS - 0x4, latest DWORD in data segment)
static uint32_t *g_main_domain = (uint32_t*)(0x00003858); // end_data + 0x3854 (bss + 0x3854), used as dword pointer, not char!!!
static uint16_t *g_loaddomain_code = (uint16_t*)(0x000074D8); // start_text + 0x74D8, LDRB R3, [R1]
#endif


// Do not send BUTTON_PRESSED event notify to "oled" if set.
// something like a mutex, but we don't really care about
// thread-safe as we only care of atomic writes which are
// in the same thread.
static int lock_buttons = 0;

// -1 means we're not inside the menu. 0 means first script.
static int current_infopage_item = -1;

// Assuming we have only one 2GHz Wi-Fi network by default.
static unsigned int page_before_information = PAGE_BEFORE_INFORMATION;

static uint32_t startcode = 0; // start of TEXT segment
// NOT REALLY A start of DATA segment
// proc(5) is incorrect, read Documentation/filesystems/proc.txt!
// https://stackoverflow.com/questions/29780731/wrong-entries-with-proc-pid-stat
static uint32_t start_data = 0;
static uint32_t end_data = 0; // end of DATA segment and start of BSS
static char dummy[100];

static char current_menu_buf[1024];

/*
 * Real handlers from oled binary and libraries
 */
static int (*register_notify_handler_real)(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig) = NULL;
static int (*notify_handler_async_real)(int subsystemid, int action, int subaction) = NULL;

/* 
 * Menu-related configuration
 */

static const char *scripts[] = {
    SCRIPT_PATH "/radio_mode.sh",
    SCRIPT_PATH "/imei_change.sh",
    SCRIPT_PATH "/ttlfix.sh",
    SCRIPT_PATH "/anticensorship.sh",
    SCRIPT_PATH "/adblock.sh",
    SCRIPT_PATH "/remote_access.sh",
    SCRIPT_PATH "/usb_mode.sh",
    SCRIPT_PATH "/wifi.sh",
    OLED_CUSTOM,
    NULL
};

static int scripts_count = 0;

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
    "RNDIS (Windows)",
    // 2
    "ECM (Linux)",
    // 3
    "Modem",
    // 4
    "Modem (NCM)",
    // 5
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

struct menu_s {
    uint8_t radio_mode;
    uint8_t imei_change;
    uint8_t ttlfix;
    uint8_t anticensorship;
    uint8_t adblock;
    uint8_t remote_access;
    uint8_t usb_mode;
    uint8_t wifi;
    uint8_t custom;
} menu_state;

/* *************************************** */

#define UNPROTECT(addr,len) (mprotect((void*)(addr-(addr%len)),len,PROT_READ|PROT_WRITE|PROT_EXEC))
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
                menu_state.imei_change = ret;
                break;
            case 2:
                menu_state.ttlfix = ret;
                break;
            case 3:
                menu_state.anticensorship = ret;
                break;
            case 4:
                menu_state.adblock = ret;
                break;
            case 5:
                menu_state.remote_access = ret;
                break;
            case 6:
                menu_state.usb_mode = ret;
                break;
            case 7:
                menu_state.wifi = ret;
                break;
            case 8:
                menu_state.custom = ret;
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
             "   > %s\n     %s\n     %s\n",
             (mapping[current_item]),
             ((char_list_size >= 2) ? mapping[current_item + 1] : nothing),
             ((char_list_size >= 3) ? mapping[current_item + 2] : nothing)
        );
    }
    else if (current_item == char_list_size - 1 && char_list_size >= 3) {
        snprintf(buf, 1024 - 1,
             "     %s\n     %s\n   > %s\n",
             ((current_item >= 2 && char_list_size > 2) ? mapping[current_item - 2] : nothing),
             ((current_item >= 1 && char_list_size > 1) ? mapping[current_item - 1] : nothing),
             (mapping[current_item])
        );
    }
    else if (current_item <= char_list_size) {
        snprintf(buf, 1024 - 1,
            "     %s\n   > %s\n     %s\n",
            ((current_item > 0) ? mapping[current_item - 1] : nothing),
            (mapping[current_item]),
            ((current_item < char_list_size && mapping[current_item + 1]) \
                                    ? mapping[current_item + 1] : nothing)
        );
    }
    else {
        snprintf(buf, 1024 - 1,
            "     ERROR\n\n\n");
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
static void continue_menu() {
    *g_current_page = page_before_information;
}

static void enter_menu() {
    current_infopage_item = 0;
};

static void exit_menu() {
    current_infopage_item = -1;
    *g_current_page = PAGE_INFORMATION;
}

static void create_and_write_menu(int menu_item) {
    char tempbuf[1024];

    switch (menu_item) {
        case 0:
            create_menu_item(current_menu_buf, network_mode_mapping, menu_state.radio_mode);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Network Mode:", current_menu_buf);
            break;
        case 1:
            create_menu_item(current_menu_buf, imei_change_mapping, menu_state.imei_change);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# IMEI (req. reboot):", current_menu_buf);
            break;
        case 2:
            create_menu_item(current_menu_buf, ttlfix_mapping, menu_state.ttlfix);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# TTL (req. reboot):", current_menu_buf);
            break;
        case 3:
            create_menu_item(current_menu_buf, enabled_disabled_mapping, menu_state.anticensorship);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Anticensorship:", current_menu_buf);
            break;
        case 4:
            create_menu_item(current_menu_buf, enabled_disabled_mapping, menu_state.adblock);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Adblock:", current_menu_buf);
            break;
        case 5:
            create_menu_item(current_menu_buf, remote_access_mapping, menu_state.remote_access);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Remote Access:", current_menu_buf);
            break;
        case 6:
            create_menu_item(current_menu_buf, usb_mode_mapping, menu_state.usb_mode);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# USB Mode:", current_menu_buf);
            break;
        case 7:
            create_menu_item(current_menu_buf, enabled_disabled_mapping, menu_state.wifi);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Wi-Fi (w/reboot):", current_menu_buf);
            break;
        case 8:
            create_menu_item(current_menu_buf, enabled_disabled_mapping, menu_state.custom);
            snprintf(tempbuf, 1024 - 1, "%s\n%s", "# Custom Script:", current_menu_buf);
            break;
    }
    fprintf(stderr, "CREATING MENU!!!!!\n");
    strncpy(current_menu_buf, tempbuf, 1024 - 1);
}

static int notify_handler_async(int subsystemid, int action, int subaction) {
    int i;
    static FILE *fp;

    if (!start_data) {
        fp = fopen("/proc/self/stat", "r");
        if (fp) {
            fscanf(fp, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld"
            /*           1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16  17  18  19  20 */
                       "%ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu"
            /*           21   22  23  24  25  26  27  28  29  30  31  32  33  34  35  36  37   */
                       "%d %d %u %u %llu %lu %ld %lu %lu",
            /*          38 39 40 41   42  43  44  45  46                                       */
                   &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,     // 10
                   &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,     // 20
                   &dummy, &dummy, &dummy, &dummy, &dummy, &startcode, &dummy, &dummy, &dummy, &dummy, // 30
                   &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,     // 40
                   &dummy, &dummy, &dummy, &dummy, &start_data, &end_data);

            // start_data is not really a start of DATA segment, but an end of previous segment.
            // There are other segments before data. We need to skip them.
            start_data = (start_data & 0xFFFFF000) + 0x00001000;

            // Add start_data to offsets
            g_current_page = (uint32_t*)(end_data + (uint32_t)g_current_page);
            g_led_status = (uint32_t*)(start_data + (uint32_t)g_led_status);
            g_main_domain = (uint32_t*)(end_data + (uint32_t)g_main_domain);
            g_loaddomain_code = (uint16_t*)(startcode + (uint32_t)g_loaddomain_code);

            // unprotecting code to patch it
            UNPROTECT((uint32_t)g_loaddomain_code, 4096);

            // patch domain char buffer and ip address buffer to act like a storage
            // for a pointer, not for the actual domain or ip address.
            // Domain buffer is 64 bytes long and IP address is 16, which is not
            // sufficient for all menu texts.
            *g_loaddomain_code = 0x6809; // ldr r1, [r1]

            fclose(fp);
        }
    }

    // store pointer to current_menu_buf there.
    *(g_main_domain) = (uint32_t)&current_menu_buf;

    if (!scripts_count) {
        for (i = 0; scripts[i] != NULL; i++) {
            scripts_count++;
        }
        if (access(OLED_CUSTOM, F_OK) != 0) {
            scripts[i-1] = NULL;
            scripts_count--;
        }
    }
    //fprintf(stderr, "notify_handler_async: %d, %d, %x\n", subsystemid, action, subaction);
    //fprintf(stderr, "!!!!!!! current page = %d !!!!!!!, led status = %d, main_domain = _%s_\n",
    //        *g_current_page, *g_led_status, g_main_domain);

    if (subsystemid == EVT_OLED_WIFI_WAKEUP) {
        // Do NOT notify "oled" of EVT_OLED_WIFI_WAKEUP event.
        // Fixes "exiting sleep mode" on every button
        // if Wi-Fi is completely disabled in web interface.
        return 0;
    }

    if (*g_current_page == PAGE_BEFORE_INFORMATION + 1) {
        // We have two Wi-Fi networks.
        page_before_information = PAGE_BEFORE_INFORMATION + 1;
        current_infopage_item = -1;
    }

    if (*g_current_page == PAGE_INFORMATION || *g_current_page == page_before_information) {
        fprintf(stderr, "PAGE_INFORMATION\n");
        if (subsystemid == SUBSYSTEM_GPIO && *g_led_status == LED_ON) {
            fprintf(stderr, "We're not on a main info screen!\n");
            if (lock_buttons) {
                // Do NOT notify "oled" of button events
                // if buttons are locked by slow script
                return 0;
            }
            if (action == BUTTON_POWER && current_infopage_item != -1) {
                // button pressed
                fprintf(stderr, "BUTTON PRESSED!\n");
                // lock buttons to prevent user intervention
                LOCKBUTTONS(1);
                handle_menu_state_change(current_infopage_item);
                update_menu_state();
                create_and_write_menu(current_infopage_item);
                LOCKBUTTONS(0);
                continue_menu();
                return notify_handler_async_real(subsystemid, BUTTON_MENU, subaction);
            }
            else if (action == BUTTON_MENU) {
                if (*g_current_page == page_before_information) {
                    // enter advanced menu if we're on an IP address page and pressed BUTTON_MENU
                    enter_menu();
                    LOCKBUTTONS(1);
                    update_menu_state();
                    create_and_write_menu(current_infopage_item);
                    LOCKBUTTONS(0);
                    return notify_handler_async_real(subsystemid, action, subaction);
                }
                current_infopage_item++;
                fprintf(stderr, "CURRENT INFOPAGE ITEM = %d, SCRIPTS COUNT = %d\n", current_infopage_item, scripts_count);
                if (current_infopage_item < scripts_count) {
                    fprintf(stderr, "GOING BACK AND RE-ENTERING MENU!\n");
                    continue_menu();
                    create_and_write_menu(current_infopage_item);
                }
                else {
                    exit_menu();
                }
            }
        }
    }
    else {
        current_infopage_item = -1;
        page_before_information = PAGE_BEFORE_INFORMATION;
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


/*int ATP_TRACE_PrintInfo(char *sourcefile, int line, int linep, char *name,
                     int offset, const char *message, ...) {
    va_list args;
    va_start(args, message);
    fprintf(stderr, "[%s] %d (%d): ", sourcefile, line, linep);
    vprintf(message, args);
    va_end(args);

    return 0;
}*/
