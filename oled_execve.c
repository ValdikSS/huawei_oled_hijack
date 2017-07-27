/*
 * Advanced OLED menu for Huawei E5372 portable LTE router.
 * LD_PRELOAD / execve() wrapper for original "oled" executable.
 * 
 * Compile:
 * arm-linux-androideabi-gcc -fPIC -O2 -s -o oled oled_execve.c
 */
#include <stdio.h>
#include <sys/types.h>

int main(int argc, char *argv[], char *envp[]) {
    char *envp_oled[100];
    char *oled_orig = "/app/bin/oled.orig";
    char *ldpreload = "LD_PRELOAD=/app/bin/oled_hijack/oled_hijack.so";
    char *args[] = {oled_orig, NULL};

    int i, status;
    pid_t pid;

    for (i = 0; envp[i] != NULL; i++) {
        envp_oled[i] = envp[i];
    }
    envp_oled[i] = ldpreload;
    envp_oled[i+1] = NULL;

    pid = fork();
    if (pid < 0) {
        return 1;
    }

    if (pid > 0) {
        waitpid(pid, &status, 0);
        return 1;
    }
    else {
        execve(oled_orig, args, envp_oled);
    }
}
