/*
 * Copyright (c) 2024 Red Hat, Inc.
 * Author: Red Hat Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef GIT_VERSION
const char* VERSION = GIT_VERSION;
#else
const char* VERSION = "v0.0.0";
#endif

#define MAX_DEVICE 1024
#define MAX_ENTRIES 1024
#define MAX_KEY 1024

const uint16_t MAX_ITERATIONS = 3;
const uint16_t MAX_PATH = 1024;
const uint16_t MAX_CONTROL_MSG = 1024;

// Time to wait before trying to write key
const uint16_t START_DELAY = 0;

typedef struct {
    char dev[MAX_DEVICE];
    char key[MAX_KEY];
} key_entry_t;
key_entry_t keys[MAX_ENTRIES];
uint16_t entry_counter = 0;
uint8_t thread_loop = 1;
uint8_t control_thread_info = 0;
pthread_mutex_t mutex;

static void
get_control_socket_name(const char* file_sock, char* control_sock, uint32_t control_sock_len) {
    char *p = strstr(file_sock, ".sock");
    size_t prefix_length = strlen(file_sock) - strlen(p);
    memset(control_sock, 0, control_sock_len);
    strncpy(control_sock, file_sock, prefix_length);
    if (prefix_length + strlen(".control.sock") < control_sock_len) {
        strcat(control_sock + prefix_length, ".control.sock");
    }
}

static void insert_device(const char* dev) {
    if(entry_counter == MAX_ENTRIES) {
        perror("No more entries accepted\n");
    }
    pthread_mutex_lock(&mutex);
    strncpy(keys[entry_counter].dev, dev, MAX_DEVICE);
    pthread_mutex_unlock(&mutex);
}

static void insert_key(const char* key) {
    if(entry_counter == MAX_ENTRIES) {
        perror("No more entries accepted\n");
    }
    pthread_mutex_lock(&mutex);
    strncpy(keys[entry_counter++].key, key, MAX_KEY);
    pthread_mutex_unlock(&mutex);
}


static const char* get_key(const char* dev) {
    for(int e = 0; e < entry_counter; e++) {
        pthread_mutex_lock(&mutex);
        if(0 == strcmp(keys[e].dev, dev)) {
            pthread_mutex_unlock(&mutex);
            return keys[e].key;
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

static void* control_thread(void *targ) {
    // Create a socket to listen on control socket
    struct sockaddr_un control_addr, accept_addr;
    int s, a, ret;
    char control_msg[MAX_CONTROL_MSG];
    const char* control_sock = (const char*)targ;
    socklen_t len;
    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sun_family = AF_UNIX;
    strcpy(control_addr.sun_path, control_sock);
    unlink(control_sock);
    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        perror("control socket");
        pthread_exit("control socket");
    }
    ret = bind(s, (struct sockaddr *)&control_addr, sizeof(control_addr));
    if (ret == -1) {
        perror("control bind");
        pthread_exit("control bind");
    }
    ret = listen(s, SOMAXCONN);
    if (ret == -1) {
        perror("control listen");
        pthread_exit("control listen");
    }
    while (thread_loop) {
        a = accept(s, (struct sockaddr *)&accept_addr, &len);
        if (a == -1) {
            perror("control accept");
            pthread_exit("control accept");
        }
        memset(control_msg, 0, MAX_CONTROL_MSG);
        recv(a, control_msg, MAX_CONTROL_MSG, 0);
        char* t = control_msg;
        int is_device = 1;
        while((t = strtok(t, ","))) {
            if (is_device) {
                printf("Adding device:%s\n", t);
                insert_device(t);
                is_device = 0;
            } else {
                printf("Adding key:%s\n", t);
                insert_key(t);
                // As long as some key is inserted, we store it in the control_thread_info variable
                control_thread_info = 1;
            }
            t = strtok(NULL, ",");
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int s, a, opt, ret;
    // The device entries
    for (uint16_t e = 0; e < MAX_ENTRIES; e++) {
        memset(&keys[e], 0, sizeof(key_entry_t));
    }
    uint32_t iterations = MAX_ITERATIONS, startdelay = START_DELAY;
    uint32_t ic, time = 0;
    char sock_file[MAX_PATH];
    char sock_control_file[MAX_PATH];
    char key[MAX_KEY];
    memset(sock_control_file, 0, MAX_PATH);
    memset(key, 0, MAX_KEY);
    struct sockaddr_un sock_addr, accept_addr, peer_addr;
    socklen_t len;
    socklen_t pathlen;

    while ((opt = getopt(argc, argv, "c:f:k:i:s:t:h")) != -1) {
        int ret_code = EXIT_FAILURE;
        switch (opt) {
        case 'c':
            strncpy(sock_control_file, optarg, MAX_PATH);
            break;
        case 'f':
            strncpy(sock_file, optarg, MAX_PATH);
            break;
        case 'k':
            strncpy(key, optarg, MAX_KEY);
            break;
        case 't':
            iterations = strtoul(optarg, 0, 10);
            break;
        case 's':
            startdelay = strtoul(optarg, 0, 10);
            break;
        case 'h':
            ret_code = EXIT_SUCCESS;
            __attribute__ ((fallthrough));
        default:
            printf("Usage: %s -f socket_file [-c control_socket] [-k key] "\
                   "[-t iterations, 3 by default] "\
                   "[-s start delay, 0s by default]\n", argv[0]);
            exit(ret_code);
        }
    }
    printf("VERSION: [%s]\n", VERSION);
    printf("KEY: [%s]\n", key);
    printf("TRY ITERATIONS: [%u]\n", iterations);
    printf("START DELAY: [%u] seconds\n", startdelay);
    printf("FILE: [%s]\n", sock_file);
    if(0 == strlen(sock_control_file) ) {
      get_control_socket_name(sock_file, sock_control_file, MAX_PATH);
    }
    printf("CONTROL FILE: [%s]\n", sock_control_file);

    pthread_t thid;
    void* tret;
    // Create control socket thread
    if (pthread_create(&thid, NULL, control_thread, sock_control_file) != 0) {
        perror("pthread_create() error");
        exit(EXIT_FAILURE);
    }

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, sock_file);
    unlink(sock_file);

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    ret = bind(s, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (ret == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    ret = listen(s, SOMAXCONN);
    if (ret == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    len = sizeof(accept_addr);

    while (ic < iterations) {
        if (time++ < startdelay && !control_thread_info) {
            sleep(1);
            printf("Start time elapsed: [%u/%u] seconds\n", time, startdelay);
            continue;
        }
        a = accept(s, (struct sockaddr *)&accept_addr, &len);
        if (a == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        pathlen = len - offsetof(struct sockaddr_un, sun_path);
        len = sizeof(peer_addr);
        ret = getpeername(a, (struct sockaddr *)&peer_addr, &len);
        if (ret == -1) {
            perror("getpeername");
            exit(EXIT_FAILURE);
        }

        pathlen = len - offsetof(struct sockaddr_un, sun_path);
        char peer[pathlen];
        memset(peer, 0, pathlen);
        strncpy(peer, peer_addr.sun_path+1, pathlen-1);
        printf("Try: [%u/%u]\n", ic, iterations);
        printf("getpeername sun_path(peer): [%s]\n", peer);
        char* t = peer;
        const char* unlocking_device;
        while((t = strtok(t, "/"))) {
            if(t) {
                unlocking_device = t;
            }
            t = strtok(NULL, ",");
        }
        printf("Trying to unlock device:[%s]\n", unlocking_device);
        // Now we have all the information in peer, something like:
        // \099226072855ae2d8/cryptsetup/luks-6e38d5e1-7f83-43cc-819a-7416bcbf9f84
        // NUL random /cryptsetup/ DEVICE
        // If we need to unencrypt device, pick it from peer information
        // To return the key, just respond to socket returned by accept
        if(strlen(key)) {
            send(a, key, strlen(key), 0);
        } else {
            const char* entry_key;
            if((entry_key = get_key(unlocking_device))) {
                send(a, entry_key, strlen(entry_key), 0);
            } else {
                printf("Device not found: [%s]\n", unlocking_device);
            }
        }
        close(a);
        ic++;
    }
    printf("Closing (max tries reached)\n");
    pthread_kill(thid, SIGKILL);
    thread_loop = 0;
    if (pthread_join(thid, &tret) != 0) {
        perror("pthread_join error");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
