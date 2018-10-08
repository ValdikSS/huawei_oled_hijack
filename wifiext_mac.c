/*
 * Wi-Fi Extender mode MAC address changer for Huawei E5770/E5885 portable LTE router.
 * Acts as a LD_PRELOAD hook library for "npdaemon" binary.
 * Changes "set_mib hwaddr" Wireless Extensions (wext) command inside snprintf_s call.
 *
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -O2 -D__ANDROID_API__=19 -s -o wifiext_mac.so wifiext_mac.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#define WIFIIFACE "WiFi0"
#define MACFILE   "/data/userdata/wifiext_mac"
#define MACLENGTH 17

static int (*vsnprintf_s_real)(void *dst, size_t siz1, size_t siz2, char *fmt, va_list data) = NULL;

// from https://stackoverflow.com/questions/9895216
static void remove_all_chars(char* str, char c) {
    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}

int snprintf_s(char *dst, size_t siz1, size_t siz2, char *fmt, ...) {
    int ret;
    FILE *fp;
    va_list argptr;
    char macaddr[MACLENGTH + 3] = "";
    static char previous_dst[10] = "";

    unsetenv("LD_PRELOAD");

    if (!vsnprintf_s_real) {
        vsnprintf_s_real = dlsym(RTLD_NEXT, "vsnprintf_s");
    }

    va_start(argptr, fmt);
    ret = vsnprintf_s_real(dst, siz1, siz2, fmt, argptr);
    va_end(argptr);

    if (strstr(previous_dst, WIFIIFACE) && strstr(dst, "hwaddr")) {
        fp = fopen(MACFILE, "r");
        if (!fp) {
            return ret;
        }
        fgets(macaddr, sizeof(macaddr), fp);
        fclose(fp);
        macaddr[strcspn(macaddr, "\r\n")] = '\0';
        remove_all_chars(macaddr, ':');
        remove_all_chars(macaddr, '-');

        snprintf(dst, siz1, "set_mib \"hwaddr=%s\"", macaddr);
    }

    strncpy(previous_dst, dst, sizeof(previous_dst) - 1);
    previous_dst[sizeof(previous_dst) - 1] = '\0';
    return ret;
}
