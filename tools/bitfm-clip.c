/*
 * bitfm-clip: Multi-MIME-type Wayland clipboard helper for BitFM
 *
 * Usage: bitfm-clip <copy|cut> <file1> [file2] ...
 *
 * Sets the Wayland clipboard to offer files in all standard MIME types:
 *   - x-special/gnome-copied-files  (Nautilus, Nemo, Thunar)
 *   - text/uri-list                 (standard drag & drop / clipboard)
 *   - text/plain                    (fallback: full absolute paths)
 *   - application/x-kde-cutselection (Dolphin)
 *
 * This solves the limitation where wl-copy can only offer one MIME type
 * and Qt6's Wayland clipboard backend doesn't expose custom MIME types.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <wayland-client.h>
#include "ext-data-control-v1.h"

/* ----- state -------------------------------------------------- */
static struct wl_display              *display;
static struct wl_seat                 *seat;
static struct ext_data_control_manager_v1 *manager;
static struct ext_data_control_device_v1  *device;
static struct ext_data_control_source_v1  *source;
static int cancelled;

/* ----- data we serve ------------------------------------------ */
static char *gnome_data;   /* "copy\nfile:///...\n"   */
static char *uri_data;     /* "file:///...\r\n"       */
static char *plain_data;   /* "/home/.../file\n"      */
static char *kde_cut_data; /* "0" or "1"              */

/* ----- helpers ------------------------------------------------ */
static char *url_encode_path(const char *path) {
    /* Encode special chars in file path for URI.
       We only need to encode spaces and a few specials. */
    size_t len = strlen(path);
    /* Worst case: every char becomes %XX = 3x */
    char *out = malloc(len * 3 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c == ' ') {
            *p++ = '%'; *p++ = '2'; *p++ = '0';
        } else if (c == '#') {
            *p++ = '%'; *p++ = '2'; *p++ = '3';
        } else if (c == '?') {
            *p++ = '%'; *p++ = '3'; *p++ = 'F';
        } else if (c == '%') {
            *p++ = '%'; *p++ = '2'; *p++ = '5';
        } else {
            *p++ = (char)c;
        }
    }
    *p = '\0';
    return out;
}

/* ----- source events ------------------------------------------ */
static void source_send(void *data,
                        struct ext_data_control_source_v1 *s,
                        const char *mime_type, int32_t fd) {
    const char *payload = NULL;
    size_t len = 0;

    if (strcmp(mime_type, "x-special/gnome-copied-files") == 0 ||
        strcmp(mime_type, "x-special/nautilus-clipboard") == 0) {
        payload = gnome_data;
    } else if (strcmp(mime_type, "text/uri-list") == 0) {
        payload = uri_data;
    } else if (strcmp(mime_type, "application/x-kde-cutselection") == 0) {
        payload = kde_cut_data;
    } else {
        /* text/plain and anything else */
        payload = plain_data;
    }

    if (payload) {
        len = strlen(payload);
        size_t written = 0;
        while (written < len) {
            ssize_t n = write(fd, payload + written, len - written);
            if (n <= 0) break;
            written += n;
        }
    }
    close(fd);
}

static void source_cancelled(void *data,
                              struct ext_data_control_source_v1 *s) {
    cancelled = 1;
    ext_data_control_source_v1_destroy(s);
}

static const struct ext_data_control_source_v1_listener source_listener = {
    .send = source_send,
    .cancelled = source_cancelled,
};

/* ----- device events ------------------------------------------ */
static void device_data_offer(void *data,
                              struct ext_data_control_device_v1 *d,
                              struct ext_data_control_offer_v1 *offer) {
    /* We don't need incoming offers */
    ext_data_control_offer_v1_destroy(offer);
}

static void device_selection(void *data,
                             struct ext_data_control_device_v1 *d,
                             struct ext_data_control_offer_v1 *offer) {
    if (offer) ext_data_control_offer_v1_destroy(offer);
}

static void device_finished(void *data,
                            struct ext_data_control_device_v1 *d) {
    cancelled = 1;
}

static void device_primary_selection(void *data,
                                     struct ext_data_control_device_v1 *d,
                                     struct ext_data_control_offer_v1 *offer) {
    if (offer) ext_data_control_offer_v1_destroy(offer);
}

static const struct ext_data_control_device_v1_listener device_listener = {
    .data_offer = device_data_offer,
    .selection = device_selection,
    .finished = device_finished,
    .primary_selection = device_primary_selection,
};

/* ----- registry ---------------------------------------------- */
static void registry_global(void *data, struct wl_registry *reg,
                            uint32_t name, const char *iface, uint32_t ver) {
    if (strcmp(iface, ext_data_control_manager_v1_interface.name) == 0) {
        manager = wl_registry_bind(reg, name,
                                   &ext_data_control_manager_v1_interface, 1);
    } else if (strcmp(iface, "wl_seat") == 0) {
        if (!seat) {
            seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *reg,
                                   uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ----- signal handler ---------------------------------------- */
static void sighandler(int sig) {
    cancelled = 1;
}

/* ----- main -------------------------------------------------- */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: bitfm-clip <copy|cut> <file1> [file2...]\n");
        return 1;
    }

    const char *action = argv[1]; /* "copy" or "cut" */
    int is_cut = (strcmp(action, "cut") == 0);
    int nfiles = argc - 2;

    /* Build payloads */
    size_t gnome_cap = 64, uri_cap = 64, plain_cap = 64;
    for (int i = 0; i < nfiles; i++) {
        size_t plen = strlen(argv[i + 2]);
        gnome_cap += plen * 3 + 32;
        uri_cap   += plen * 3 + 32;
        plain_cap += plen + 4;
    }

    gnome_data = calloc(1, gnome_cap);
    uri_data   = calloc(1, uri_cap);
    plain_data = calloc(1, plain_cap);

    /* gnome-copied-files: "copy\nfile:///path\nfile:///path2\n" */
    strcat(gnome_data, is_cut ? "cut\n" : "copy\n");

    for (int i = 0; i < nfiles; i++) {
        char *encoded = url_encode_path(argv[i + 2]);

        char uri[8192];
        snprintf(uri, sizeof(uri), "file://%s", encoded);

        /* gnome payload */
        strcat(gnome_data, uri);
        strcat(gnome_data, "\n");

        /* uri-list payload (CRLF per RFC 2483) */
        strcat(uri_data, uri);
        strcat(uri_data, "\r\n");

        /* plain text (newline separated full paths) */
        strcat(plain_data, argv[i + 2]);
        if (i < nfiles - 1) strcat(plain_data, "\n");

        free(encoded);
    }

    kde_cut_data = is_cut ? "1" : "0";

    /* Daemonize: fork into background so the caller doesn't block */
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid > 0) {
        /* Parent exits immediately */
        return 0;
    }
    /* Child continues as clipboard data source */
    setsid();
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /* Connect to Wayland */
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "bitfm-clip: cannot connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!manager) {
        fprintf(stderr, "bitfm-clip: compositor does not support ext-data-control-v1\n");
        wl_display_disconnect(display);
        return 1;
    }
    if (!seat) {
        fprintf(stderr, "bitfm-clip: no seat found\n");
        wl_display_disconnect(display);
        return 1;
    }

    /* Create data source and offer all MIME types */
    source = ext_data_control_manager_v1_create_data_source(manager);
    ext_data_control_source_v1_add_listener(source, &source_listener, NULL);

    ext_data_control_source_v1_offer(source, "x-special/gnome-copied-files");
    ext_data_control_source_v1_offer(source, "x-special/nautilus-clipboard");
    ext_data_control_source_v1_offer(source, "text/uri-list");
    ext_data_control_source_v1_offer(source, "text/plain");
    ext_data_control_source_v1_offer(source, "text/plain;charset=utf-8");
    ext_data_control_source_v1_offer(source, "application/x-kde-cutselection");

    /* Get data device for the seat and set selection */
    device = ext_data_control_manager_v1_get_data_device(manager, seat);
    ext_data_control_device_v1_add_listener(device, &device_listener, NULL);
    ext_data_control_device_v1_set_selection(device, source);
    wl_display_roundtrip(display);

    /* Event loop: serve paste requests until cancelled */
    while (!cancelled && wl_display_dispatch(display) != -1) {
        /* keep running */
    }

    /* Cleanup */
    ext_data_control_device_v1_destroy(device);
    ext_data_control_manager_v1_destroy(manager);
    wl_display_disconnect(display);

    free(gnome_data);
    free(uri_data);
    free(plain_data);

    return 0;
}
