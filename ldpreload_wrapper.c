/*
 * LD_PRELOAD / execve() wrapper for original "oled" executable.
 * 
 * Compile for oled:
 * arm-linux-androideabi-gcc -s -fPIC -O2 -D__ANDROID_API__=9 \
 * -DBINARY='"/app/bin/oled.orig"' -DPRELOADLIB='"/app/bin/oled_hijack/oled_hijack.so"' \
 * -o oled ldpreload_wrapper.c
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef BINARY
#error "You should set BINARY -DBINARY='"/path/to/binfile"' "
#endif

#ifndef PRELOADLIB
#error "You should set PRELOADLIB -DPRELOADLIB='"/path/to/lib.so"' "
#endif

int main(int argc, char *argv[], char *envp[]) {
    unsigned int i;
    char *envp_preload[100] = {0};
    char *binary_orig = BINARY;
    char *ldpreload = "LD_PRELOAD=" PRELOADLIB;
    char *args[] = {argv[0], NULL};

    for (i = 0; envp[i] != NULL; i++) {
        if (i >= sizeof(envp_preload)/sizeof(*envp_preload))
            return 1;
        envp_preload[i] = envp[i];
    }
    envp_preload[i] = ldpreload;
    envp_preload[i+1] = NULL;

    return execve(binary_orig, args, envp_preload);
}
