/*
 * Wi-Fi web function hook Huawei E5770/E5885 portable LTE router.
 * 
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -pthread -DHOOK -O2 -D__ANDROID_API__=19 -s -o wifi_hook.so wifi_hook.c
 * arm-linux-androideabi-gcc -fPIC -DCLIENT -O2 -D__ANDROID_API__=19 -s -o wifi_hook_client wifi_hook.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#define SUBSYSTEM_WLAN 16
#define SOCK_NAME "/var/wifihook"

#if !defined(HOOK) && !defined(CLIENT)
#error "You should define either -DHOOK or -DCLIENT"
#endif

#ifdef HOOK
static int (*webserver_r_h_real)(
    int subsystemnum, const char *subsystemname,
    void* hookfunction, void *global_release_msg) = NULL;

static int (*wififunc)(
    const char *function_name, int req_type_get_post,
    char *req_body, size_t req_size) = NULL;

static int create_socket(char* path) {
    struct sockaddr_un addr;
    int fd;

    unlink(SOCK_NAME);
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Can't open socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Can't bind socket");
        exit(EXIT_FAILURE);
    }
    if (listen(fd, 5) == -1) {
        perror("Can't listen socket");
        exit(EXIT_FAILURE);
    }

    return fd;
}

static void* wifi_hookserver(void* nothing) {
    int fd, client;
    ssize_t rsize;
    char buf[8192];
    char libfunction[64];
    char *c_token = NULL;
    int reqtype = 1;
    void* ret;

    fd = create_socket(SOCK_NAME);

    for (;;) {
        if ((client = accept(fd, NULL, NULL)) == -1) {
            continue;
        }
        while ((rsize=read(client, buf, sizeof(buf) - 1)) > 0) {
            buf[rsize] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if ((c_token = strtok(buf, "|")) != NULL) {
                strncpy(libfunction, c_token, sizeof(libfunction) - 1);
                libfunction[sizeof(libfunction) - 1] = '\0';

                if ((c_token = strtok(NULL, "|")) == NULL) {
                    close(client);
                    continue;
                }
                reqtype = 1;
                reqtype = atoi(c_token);

                if ((c_token = strtok(NULL, "|")) == NULL) {
                    close(client);
                    continue;
                }
                ret = (void*)wififunc(libfunction, reqtype, c_token, strlen(c_token));
                if (ret) {
                    dprintf(client, "%s\n", ret);
                    if (strstr(ret, "<error>") == NULL &&
                        strstr(ret, "<response>OK</response>") == NULL)
                    {
                        free(ret);
                    }
                }
                close(client);
            }
        }
        close(client);
    }
}


int webserver_register_hookfunction(int subsystemnum, const char *subsystemname,
                                    void* hookfunction, void *global_release_msg)
{
    static pthread_t wifihookthread;
    unsetenv("LD_PRELOAD");

    if (!webserver_r_h_real) {
        webserver_r_h_real = dlsym(RTLD_NEXT, "webserver_register_hookfunction");
    }

    if (subsystemnum == SUBSYSTEM_WLAN && !wififunc) {
        wififunc = hookfunction;
        if (pthread_create(&wifihookthread, NULL, wifi_hookserver, NULL)) {
            perror("Error creating thread.");
        }
    }

    return webserver_r_h_real(subsystemnum, subsystemname, hookfunction, global_release_msg);
}
#endif


#ifdef CLIENT
static int open_socket(char* path) {
    struct sockaddr_un addr;
    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Can't open socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Can't connect");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int main(int argc, char* argv[]) {
    int fd;
    ssize_t rsize;
    char buf[8192];
    
    if (argc != 4) {
        puts("Need 3 arguments: <funcname> <1 for get, 2 for post> <data>");
        exit(EXIT_FAILURE);
    }

    fd = open_socket(SOCK_NAME);
    dprintf(fd, "%s|%s|%s\n", argv[1], argv[2], argv[3]);
    rsize = read(fd, buf, sizeof(buf) - 1);
    buf[rsize] = '\0';
    puts(buf);
    close(fd);

    return 0;
}
#endif
