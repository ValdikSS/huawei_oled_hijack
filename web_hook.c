/*
 * Web functions hook for Huawei portable LTE routers.
 * This hook allows to call web interface API functions from "the inside", without
 * authentication.
 * Both GET and POST requests are supported.
 *
 * You can call every /api/ handlers, e.g. for
 * GET http://192.168.8.1/api/wlan/station-information
 * call
 * ./client wlan station-information 1 1
 *
 * Used to get radio mode configuration (./client net net-mode 1 1),
 * toggle Wi-Fi Extender mode (./client wlan handover-setting 1 1) etc.
 *
 * Compile:
 * arm-linux-androideabi-gcc -shared -ldl -fPIC -pthread -DHOOK -DSOCK_NAME='"/var/webhook"' -O2 -D__ANDROID_API__=19 -s -o web_hook.so web_hook.c
 * arm-linux-androideabi-gcc -fPIC -DCLIENT -DSOCK_NAME='"/var/webhook"' -O2 -D__ANDROID_API__=19 -s -o web_hook_client web_hook.c
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

// UNIX socket name to listen/connect to
#if !defined(SOCK_NAME)
#error "You should define SOCK_NAME"
#endif

#if !defined(HOOK) && !defined(CLIENT)
#error "You should define either -DHOOK or -DCLIENT"
#endif

#define BUFSIZE 8192

// Building web_hook.so
#ifdef HOOK
#define HOOK_NUMBER 50
struct webhook_function_s {
    const char* subsystemname;
    void* hookfunction;
};
static struct webhook_function_s webhook_functions[HOOK_NUMBER];
static int webhook_functions_count = 0;

// Pointer to real "webserver_register_hookfunction" function
static void* (*webserver_r_h_real)(
    int subsystemnum, const char *subsystemname,
    void* hookfunction, void* global_release_msg) = NULL;

// Pointer to real "global_release_msg" function
static void (*global_release_msg_real)(void *ptr) = NULL;

// Search for registered web handlers and return pointer to hook function if found
static void* int_get_webhook(const char* name) {
    int i;

    if (!webhook_functions_count)
        return NULL;
    if (!name || strlen(name) == 0)
        return NULL;
    for (i = 0; i < webhook_functions_count; i++) {
        if (strcmp(name, webhook_functions[i].subsystemname) == 0) {
            fprintf(stderr, "[int_get_webhook] found func %d %s\n", i,
                webhook_functions[i].subsystemname);
            return webhook_functions[i].hookfunction;
        }
    }
    fprintf(stderr, "[int_get_webhook] %s not found\n", name);
    return NULL;
}

// Register hookfunction for given subsystemname
static int int_register_webhook(const char *subsystemname,
                                void* hookfunction)
{
    if (webhook_functions_count >= HOOK_NUMBER)
        return 0;

    webhook_functions[webhook_functions_count].subsystemname = strdup(subsystemname);
    webhook_functions[webhook_functions_count].hookfunction = hookfunction;
    webhook_functions_count++;
    fprintf(stderr, "[int_register_webhook] "
                    "Webhook for %s registered successfully!\n", subsystemname);

    return 1;
}

static int create_socket(char* path) {
    struct sockaddr_un addr;
    int fd;

    unlink(path);
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

// Socket server handler
static void* web_hookserver(void* nothing) {
    int fd, client;
    ssize_t rsize;
    char buf[BUFSIZE];
    char subsystemname[32];
    char libfunction[64];
    char *c_token = NULL;
    int reqtype = 1;
    void* ret;
    void* (*webfunc)(const char *function_name,
                      int req_type_get_post,
                      char *req_body,
                      size_t req_size) = NULL;

    fd = create_socket(SOCK_NAME);
    fprintf(stderr, "Created socket\n");

    for (;;) {
        if ((client = accept(fd, NULL, NULL)) == -1) {
            continue;
        }
        while ((rsize=read(client, buf, sizeof(buf) - 2)) > 0) {
            buf[rsize] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if ((c_token = strtok(buf, "|")) != NULL) {
                strncpy(subsystemname, c_token, sizeof(subsystemname) - 1);
                subsystemname[sizeof(subsystemname) - 1] = '\0';
                if (!int_get_webhook(subsystemname)) {
                    close(client);
                    continue;
                }
                webfunc = int_get_webhook(subsystemname);

                if ((c_token = strtok(NULL, "|")) == NULL) {
                    close(client);
                    continue;
                }
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
                ret = webfunc(libfunction, reqtype, c_token, strlen(c_token));
                if (ret) {
                    dprintf(client, "%s\n", ret);
                    global_release_msg_real(ret);
                }
                close(client);
            }
        }
        close(client);
    }
    return 0;
}


void* webserver_register_hookfunction(int subsystemnum, const char *subsystemname,
                                    void* hookfunction, void *global_release_msg)
{
    static pthread_t webhookthread = {0};
    unsetenv("LD_PRELOAD");

    if (!webserver_r_h_real) {
        webserver_r_h_real = dlsym(RTLD_NEXT, "webserver_register_hookfunction");
    }

    fprintf(stderr, "Trying to search for webhook %s\n", subsystemname);
    if (!int_get_webhook(subsystemname)) {
        fprintf(stderr, "Webhook not found, registering\n");
        int_register_webhook(subsystemname, hookfunction);
        fprintf(stderr, "Registered\n");
    }

    fprintf(stderr, "Checking global release\n");
    if (!global_release_msg_real) {
        fprintf(stderr, "No global release, saving\n");
        global_release_msg_real = global_release_msg;
    }

    fprintf(stderr, "Checking for webhook thread\n");
    if (!webhookthread) {
        fprintf(stderr, "No webhook thread, creating one\n");
        if (pthread_create(&webhookthread, NULL, web_hookserver, NULL)) {
            perror("Error creating thread.");
        }
    }

    return webserver_r_h_real(subsystemnum, subsystemname, hookfunction, global_release_msg);
}
#endif

// Building web_hook_client
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
    char buf[BUFSIZE];

    if (argc != 5) {
        puts("Need 4 arguments: <subsystemname> <funcname> <1 for get, 2 for post> <data>");
        exit(EXIT_FAILURE);
    }

    fd = open_socket(SOCK_NAME);
    dprintf(fd, "%s|%s|%s|%s\n", argv[1], argv[2], argv[3], argv[4]);
    rsize = read(fd, buf, sizeof(buf) - 2);
    buf[rsize] = '\0';
    puts(buf);
    close(fd);

    return 0;
}
#endif
