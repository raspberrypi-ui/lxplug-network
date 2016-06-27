/*
 * dhcpcd-gtk
 * Copyright 2009-2010 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DHCPCD_GTK_H
#define DHCPCD_GTK_H

#include <stdbool.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libintl.h>

#include "dhcpcd.h"
#include "queue.h"
#include "plugin.h"

//#define PACKAGE "dhcpcd-gtk"

#define UNCONST(a)              ((void *)(unsigned long)(const void *)(a))

#ifdef __GNUC__
#  define _unused __attribute__((__unused__))
#else
#  define _unused
#endif

typedef struct wi_menu {
    TAILQ_ENTRY(wi_menu) next;
    DHCPCD_WI_SCAN *scan;
    bool associated;
    GtkWidget *menu;
    GtkWidget *ssid;
    GtkWidget *icon;
    GtkWidget *strength;
} WI_MENU;
typedef TAILQ_HEAD(wi_menu_head, wi_menu) WI_MENUS;

typedef struct wi_scan {
    TAILQ_ENTRY(wi_scan) next;
    DHCPCD_IF *interface;
    DHCPCD_WI_SCAN *scans;

    GtkWidget *ifmenu;
    GtkWidget *sep;
    GtkWidget *noap;
    WI_MENUS menus;
} WI_SCAN;

typedef TAILQ_HEAD(wi_scan_head, wi_scan) WI_SCANS;

typedef struct watch {
    gpointer ref;
    int fd;
    guint eventid;
    GIOChannel *gio;
    struct watch *next;
} watch;

typedef struct {

    GtkWidget *plugin;              /* Back pointer to the widget */
    LXPanel *panel;                 /* Back pointer to panel */
    GtkWidget *tray_icon;           /* Displayed image */
    config_setting_t *settings;     /* Plugin settings */

    DHCPCD_CONNECTION *con;         /* Global connection data */

    /* Main globals */
    guint ani_timer;
    int ani_counter;
    bool online;
    bool carrier;
    struct watch *watches;
    WI_SCANS wi_scans;

    /* Timer handles */
    guint bgscan_timer;
    guint defscan_timer;
    guint reopen_timer;
    guint wpa_reopen_timer;

    /* Menu */
    GtkWidget *menu;

    /* Preference dialog */
    GtkWidget *dialog, *blocks, *names, *controls, *clear, *rebind;
    GtkWidget *autoconf, *address, *router, *dns_servers, *dns_search;
    DHCPCD_OPTION *config;
    char *block, *name;
    DHCPCD_IF *iface;
    char **ifaces;

    /* WPA dialog */
    GtkWidget *wpa_dialog, *wpa_err;

} DHCPCDUIPlugin;

const char *get_strength_icon_name(int strength);
void set_icon (LXPanel *p, GtkWidget *image, const char *icon, int size);

void menu_init(GtkButton *, DHCPCD_CONNECTION *);
void menu_update_scans(WI_SCAN *, DHCPCD_WI_SCAN *, GtkWidget *);
void menu_remove_if(WI_SCAN *, DHCPCDUIPlugin *);
void menu_show (DHCPCDUIPlugin *);

void notify_close(void);

GtkWidget *prefs_show(DHCPCDUIPlugin *);
void prefs_abort(DHCPCDUIPlugin *);
void menu_abort(DHCPCDUIPlugin *);
void wpa_abort(DHCPCDUIPlugin *);

bool wpa_configure(DHCPCD_WPA *, DHCPCD_WI_SCAN *);
bool wpa_disconnect (DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *scan);

#if GTK_MAJOR_VERSION == 2
GtkWidget *gtk_box_new(GtkOrientation, gint);
#endif

#endif
