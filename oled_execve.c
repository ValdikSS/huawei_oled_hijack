/*
 * Advanced OLED menu for Huawei E5372 portable LTE router.
 * LD_PRELOAD / execve() wrapper for original "oled" executable.
 * 
 * Compile:
 * arm-linux-androideabi-gcc -fPIC -O2 -s -o oled oled_execve.c
 */
#include <stdio.h>

void main() {
    char *const args[] = {"/app/bin/oled.orig", NULL};
    char *const envs[] = {"LD_PRELOAD=/app/bin/oled_hijack/oled_hijack.so",
        "LD_LIBRARY_PATH=/vendor/lib:/system/lib:/app/lib",
        "ANDROID_ROOT=/system",
        "PATH=/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin:/app/bin",
        "ANDROID_DATA=/data",
        NULL};
    execve("/app/bin/oled.orig", args, envs);
}
