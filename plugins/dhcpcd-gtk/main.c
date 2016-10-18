/*
 * dhcpcd-gtk
 * Copyright 2009-2015 Roy Marples <roy@marples.name>
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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#ifdef NOTIFY
#  include <libnotify/notify.h>
#ifndef NOTIFY_CHECK_VERSION
#  define NOTIFY_CHECK_VERSION(a,b,c) 0
#endif
static NotifyNotification *nn;
#endif

#include "config.h"
#include "dhcpcd.h"
#include "dhcpcd-gtk.h"

#include "plugin.h"

#define ICON_BUTTON_TRIM 4

static gboolean dhcpcd_try_open(gpointer data);
static gboolean dhcpcd_wpa_try_open(gpointer data);

void set_icon (LXPanel *p, GtkWidget *image, const char *icon, int size)
{
    GdkPixbuf *pixbuf;
    if (size == 0) size = panel_get_icon_size (p) - ICON_BUTTON_TRIM;
    if (gtk_icon_theme_has_icon (panel_get_icon_theme (p), icon))
    {
        GtkIconInfo *info = gtk_icon_theme_lookup_icon (panel_get_icon_theme (p), icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
        pixbuf = gtk_icon_info_load_icon (info, NULL);
        gtk_icon_info_free (info);
        if (pixbuf != NULL)
        {
            gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
            g_object_unref (pixbuf);
            return;
        }
    }
    else
    {
        char path[256];
        sprintf (path, "%s/images/%s.png", PACKAGE_DATA_DIR, icon);
        pixbuf = gdk_pixbuf_new_from_file_at_scale (path, size, size, TRUE, NULL);
        if (pixbuf != NULL)
        {
            gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
            g_object_unref (pixbuf);
        }
    }
}

const char *
get_strength_icon_name(int strength)
{

    if (strength > 80)
        return "network-wireless-connected-100";
    else if (strength > 55)
        return "network-wireless-connected-75";
    else if (strength > 30)
        return "network-wireless-connected-50";
    else if (strength > 5)
        return "network-wireless-connected-25";
    else
        return "network-wireless-connected-00";
}

static DHCPCD_WI_SCAN *
get_strongest_scan(GtkWidget *p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data(p);
    WI_SCAN *w;
    DHCPCD_WI_SCAN *scan, *s;

    scan = NULL;
    TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
        for (s = w->scans; s; s = s->next) {
            if (dhcpcd_wi_associated(w->interface, s) &&
                (scan == NULL ||
                s->strength.value > scan->strength.value))
                scan = s;
        }
    }
    return scan;
}

static gboolean
animate_carrier(gpointer p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data((GtkWidget *) p);
    const char *icon;
    DHCPCD_WI_SCAN *scan;

    if (dhcp->ani_timer == 0)
        return false;

    scan = get_strongest_scan(p);
    if (scan) {
        switch(dhcp->ani_counter++) {
        case 0:
            icon = "network-wireless-connected-00";
            break;
        case 1:
            icon = "network-wireless-connected-25";
            break;
        case 2:
            icon = "network-wireless-connected-50";
            break;
        case 3:
            icon = "network-wireless-connected-75";
            break;
        default:
            icon = "network-wireless-connected-100";
            dhcp->ani_counter = 0;
        }

    } else {
        switch(dhcp->ani_counter++) {
        case 0:
            icon = "network-transmit";
            break;
        case 1:
            icon = "network-receive";
            break;
        default:
            icon = "network-idle";
            dhcp->ani_counter = 0;
            break;
        }
    }
    set_icon (dhcp->panel, dhcp->tray_icon, icon, 0);
    return true;
}

static gboolean
animate_online(gpointer p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data((GtkWidget *) p);
    const char *icon;
    DHCPCD_WI_SCAN *scan;

    if (dhcp->ani_timer == 0)
        return false;

    if (dhcp->ani_counter++ > 6) {
        dhcp->ani_timer = 0;
        dhcp->ani_counter = 0;
        return false;
    }

    scan = get_strongest_scan(p);
    if (dhcp->ani_counter % 2 == 0)
        icon = scan ? "network-wireless-connected-00" :
            "network-idle";
    else
        icon = scan ? get_strength_icon_name(scan->strength.value) :
            "network-transmit-receive";
    set_icon (dhcp->panel, dhcp->tray_icon, icon, 0);
    return true;
}

static void
update_online(DHCPCD_CONNECTION *con, bool showif, gpointer p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data((GtkWidget *) p);
    bool ison, iscarrier;
    char *msg, *msgs, *tmp;
    DHCPCD_IF *ifs, *i;

    ison = iscarrier = false;
    msgs = NULL;
    ifs = dhcpcd_interfaces(con);
    for (i = ifs; i; i = i->next) {
        if (i->type == DHT_LINK) {
            if (i->up)
                iscarrier = true;
        } else {
            if (i->up)
                ison = true;
        }
        msg = dhcpcd_if_message(i, NULL);
        if (msg) {
            if (showif)
                g_message("%s", msg);
            if (msgs) {
                tmp = g_strconcat(msgs, "\n", msg, NULL);
                g_free(msgs);
                g_free(msg);
                msgs = tmp;
            } else
                msgs = msg;
        } else if (showif)
            g_message("%s: %s", i->ifname, i->reason);
    }

  if (dhcp->online != ison || dhcp->carrier != iscarrier || (!dhcp->online && !dhcp->carrier)) {
        dhcp->online = ison;
        dhcp->carrier = iscarrier;
        if (dhcp->ani_timer != 0) {
            g_source_remove(dhcp->ani_timer);
            dhcp->ani_timer = 0;
            dhcp->ani_counter = 0;
        }
        if (ison) {
            animate_online(p);
            dhcp->ani_timer = g_timeout_add(300, animate_online, p);
        } else if (iscarrier) {
            animate_carrier(p);
            dhcp->ani_timer = g_timeout_add(500, animate_carrier, p);
        } else {
            set_icon (dhcp->panel, dhcp->tray_icon, "network-offline", 0);
        }
  } else {
		const char *icon;
		DHCPCD_WI_SCAN *scan;
		scan = get_strongest_scan(p);
		icon = scan ? get_strength_icon_name(scan->strength.value) :
			"network-transmit-receive";
		set_icon (dhcp->panel, dhcp->tray_icon, icon, 0);
  }

    gtk_widget_set_tooltip_text(dhcp->tray_icon, msgs);
    g_free(msgs);
}

void
notify_close(void)
{
#ifdef NOTIFY
    if (nn != NULL)
        notify_notification_close(nn, NULL);
#endif
}

#ifdef NOTIFY
static char *notify_last_msg;

static void
notify_closed(void)
{
    nn = NULL;
}

static void
notify(const char *title, const char *msg, const char *icon)
{

    if (msg == NULL)
        return;
    /* Don't spam the same message */
    if (notify_last_msg) {
        if (notify_last_msg && strcmp(msg, notify_last_msg) == 0)
            return;
        g_free(notify_last_msg);
    }
    notify_last_msg = g_strdup(msg);

    if (nn != NULL)
        notify_notification_close(nn, NULL);

#if NOTIFY_CHECK_VERSION(0,7,0)
    nn = notify_notification_new(title, msg, icon);
    notify_notification_set_hint(nn, "transient",
        g_variant_new_boolean(TRUE));
#else
    if (gtk_status_icon_get_visible(status_icon))
        nn = notify_notification_new_with_status_icon(title,
            msg, icon, status_icon);
    else
        nn = notify_notification_new(title, msg, icon, NULL);
#endif

    notify_notification_set_timeout(nn, 5000);
    g_signal_connect(nn, "closed", G_CALLBACK(notify_closed), NULL);
    notify_notification_show(nn, NULL);
}
#else
#  define notify(a, b, c)
#endif

static struct watch *
dhcpcd_findwatch(int fd, gpointer data, struct watch **last, struct watch *watches)
{
    struct watch *w;

    if (last)
        *last = NULL;
    for (w = watches; w; w = w->next) {
        if (w->fd == fd || w->ref == data)
            return w;
        if (last)
            *last = w;
    }
    return NULL;
}

static void
dhcpcd_unwatch(int fd, gpointer data, struct watch **watches)
{
    struct watch *w, *l;

    if ((w = dhcpcd_findwatch(fd, data, &l, *watches))) {
        if (l)
            l->next = w->next;
        else
            *watches = w->next;
        g_source_remove(w->eventid);
        g_io_channel_unref(w->gio);
        g_free(w);
    }
}

static gboolean
dhcpcd_watch(int fd,
    gboolean (*cb)(GIOChannel *, GIOCondition, gpointer),
    gpointer data, struct watch **watches)
{
    struct watch *w, *l;
    GIOChannel *gio;
    GIOCondition flags;
    guint eventid;

    /* Sanity */
    if ((w = dhcpcd_findwatch(fd, data, &l, *watches))) {
        if (w->fd == fd)
            return TRUE;
        if (l)
            l->next = w->next;
        else
            *watches = w->next;
        g_source_remove(w->eventid);
        g_io_channel_unref(w->gio);
        g_free(w);
    }

    gio = g_io_channel_unix_new(fd);
    if (gio == NULL) {
        g_warning(_("Error creating new GIO Channel\n"));
        return FALSE;
    }
    flags = G_IO_IN | G_IO_ERR | G_IO_HUP;
    if ((eventid = g_io_add_watch(gio, flags, cb, data)) == 0) {
        g_warning(_("Error creating watch\n"));
        g_io_channel_unref(gio);
        return FALSE;
    }

    w = g_try_malloc(sizeof(*w));
    if (w == NULL) {
        g_warning(_("g_try_malloc\n"));
        g_source_remove(eventid);
        g_io_channel_unref(gio);
        return FALSE;
    }

    w->ref = data;
    w->fd = fd;
    w->eventid = eventid;
    w->gio = gio;
    w->next = *watches;
    *watches = w;

    return TRUE;
}

static void
dhcpcd_status_cb(DHCPCD_CONNECTION *con,
    unsigned int status, const char *status_msg, gpointer p)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) p;
    static unsigned int last = DHC_UNKNOWN;
    const char *msg;
    bool refresh;
    WI_SCAN *w;

    g_message(_("Status changed to %s"), status_msg);
    if (status == DHC_DOWN) {
        msg = N_(last == DHC_UNKNOWN ?
            _("Connection to dhcpcd lost") : _("dhcpcd not running"));
        if (dhcp->ani_timer != 0) {
            g_source_remove(dhcp->ani_timer);
            dhcp->ani_timer = 0;
            dhcp->ani_counter = 0;
        }
        dhcp->online = dhcp->carrier = false;
        set_icon (dhcp->panel, dhcp->tray_icon, "network-offline", 0);
        gtk_widget_set_tooltip_text(dhcp->tray_icon, msg);
        prefs_abort(dhcp);
        menu_abort(dhcp);
        wpa_abort(dhcp);
        while ((w = TAILQ_FIRST(&dhcp->wi_scans))) {
            TAILQ_REMOVE(&dhcp->wi_scans, w, next);
            dhcpcd_wi_scans_free(w->scans);
            g_free(w);
        }
        dhcpcd_unwatch(-1, con, &dhcp->watches);
        if (!dhcp->reopen_timer)
            dhcp->reopen_timer = g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_try_open, dhcp);
    } else {
        if (last == DHC_UNKNOWN || last == DHC_DOWN) {
            g_message(_("Connected to %s-%s"), "dhcpcd",
                dhcpcd_version(con));
            refresh = true;
        } else
            refresh = last == DHC_OPENED ? true : false;
        update_online(con, refresh, dhcp->plugin);
    }

    last = status;
}

static gboolean
dhcpcd_cb(_unused GIOChannel *gio, _unused GIOCondition c, gpointer data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) data;
    DHCPCD_CONNECTION *con = dhcp->con;

    if (dhcpcd_get_fd(con) == -1) {
        g_warning(_("dhcpcd connection lost"));
        dhcpcd_unwatch(-1, con, &dhcp->watches);
        if (!dhcp->reopen_timer)
            dhcp->reopen_timer = g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_try_open, data);
        return FALSE;
    }

    dhcpcd_dispatch(con);
    return TRUE;
}

static gboolean
dhcpcd_try_open(gpointer data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) data;
    DHCPCD_CONNECTION *con = dhcp->con;
    int fd;
    static int last_error;

    fd = dhcpcd_open(con, true);
    if (fd == -1) {
        if (errno == EACCES || errno == EPERM) {
            if ((fd = dhcpcd_open(con, false)) != -1)
                goto unprived;
        }
        if (errno != last_error) {
            g_critical("dhcpcd_open: %s", strerror(errno));
            last_error = errno;
        }
        return TRUE;
    }

unprived:
    if (!dhcpcd_watch(fd, dhcpcd_cb, data, &dhcp->watches)) {
        dhcpcd_close(con);
        return TRUE;
    }

    /* Start listening to WPA events */
    dhcpcd_wpa_start(con);
    dhcp->reopen_timer = 0;

    return FALSE;
}

static void
dhcpcd_if_cb(DHCPCD_IF *i, gpointer p)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) p;
    DHCPCD_CONNECTION *con;
    char *msg;
    //const char *icon;
    bool new_msg;

    /* We should ignore renew and stop so we don't annoy the user */
    if (i->state != DHS_RENEW &&
        i->state != DHS_STOP && i->state != DHS_STOPPED &&
        i->state != DHS_ROUTERADVERT)
    {
        msg = dhcpcd_if_message(i, &new_msg);
        if (msg) {
            g_message("%s", msg);
#ifdef NOTIFY
            if (new_msg) {
                if (i->up)
                    icon = "network-transmit-receive";
                //else
                //  icon = "network-transmit";
                if (!i->up)
                    icon = "network-offline";
                notify(_("Network event"), msg, icon);
            }
#endif
            g_free(msg);
        }
    }

    /* Update the tooltip with connection information */
    con = dhcpcd_if_connection(i);
    if (g_strcmp0(i->reason, "ROUTERADVERT")) update_online(con, false, dhcp->plugin);

    if (dhcpcd_is_wireless (i)) {
        DHCPCD_WI_SCAN *scans;
        WI_SCAN *w;

        TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
            if (w->interface == i)
                break;
        }
        if (w) {
            scans = dhcpcd_wi_scans(i);
            menu_update_scans(w, scans, dhcp->plugin);
        }
    }
}

static gboolean
dhcpcd_wpa_cb(_unused GIOChannel *gio, _unused GIOCondition c,
    gpointer data)
{
    DHCPCD_WPA *wpa = (DHCPCD_WPA *)data;
    DHCPCDUIPlugin *dhcp = (DHCPCDUIPlugin *) dhcpcd_wpa_get_context (wpa);
    DHCPCD_IF *i;

    if (dhcpcd_wpa_get_fd(wpa) == -1) {
        dhcpcd_unwatch(-1, wpa, &dhcp->watches);

        /* If the interface hasn't left, try re-opening */
        i = dhcpcd_wpa_if(wpa);
        if (i == NULL ||
            i->state == DHS_DEPARTED || i->state == DHS_STOPPED)
            return TRUE;
        g_warning(_("dhcpcd WPA connection lost: %s"), i->ifname);
        if (!dhcp->wpa_reopen_timer)
            dhcp->wpa_reopen_timer = g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_wpa_try_open, wpa);
        //g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_wpa_try_open, wpa);
        return FALSE;
    }

    dhcpcd_wpa_dispatch(wpa);
    return TRUE;
}

static gboolean
dhcpcd_wpa_try_open(gpointer data)
{
    DHCPCD_WPA *wpa = (DHCPCD_WPA *)data;
    DHCPCDUIPlugin *dhcp = (DHCPCDUIPlugin *) dhcpcd_wpa_get_context (wpa);
    int fd;
    static int last_error;

    fd = dhcpcd_wpa_open(wpa);
    if (fd == -1) {
        if (errno != last_error)
            g_critical("dhcpcd_wpa_open: %s", strerror(errno));
        last_error = errno;
        return TRUE;
    }

    if (!dhcpcd_watch(fd, dhcpcd_wpa_cb, wpa, &dhcp->watches)) {
        dhcpcd_wpa_close(wpa);
        return TRUE;
    }

    dhcp->wpa_reopen_timer = 0;
    return FALSE;
}

static void
dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, gpointer p)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) p;
    DHCPCD_IF *i;
    WI_SCAN *w;
    DHCPCD_WI_SCAN *scans, *s1, *s2;
    char *txt, *t;
    int lerrno, fd;
    const char *msg;

    /* This could be a new WPA so watch it */
    fd = dhcpcd_wpa_get_fd(wpa);
    if (fd == -1) {
        g_critical("No fd for WPA %p", wpa);
        dhcpcd_unwatch(-1, wpa, &dhcp->watches);
        return;
    }
    dhcpcd_wpa_set_context (wpa, p);
    dhcpcd_watch(fd, dhcpcd_wpa_cb, wpa, &dhcp->watches);

    i = dhcpcd_wpa_if(wpa);
    if (i == NULL) {
        g_critical("No interface for WPA %p", wpa);
        return;
    }
    g_message(_("%s: Received scan results"), i->ifname);
    lerrno = errno;
    errno = 0;
    scans = dhcpcd_wi_scans(i);
    if (scans == NULL && errno)
        g_warning("%s: %s", i->ifname, strerror(errno));
    errno = lerrno;
    TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
        if (w->interface == i)
            break;
    }
    if (w == NULL) {
        w = g_malloc(sizeof(*w));
        w->interface = i;
        w->scans = scans;
        w->ifmenu = NULL;
        w->sep = NULL;
        w->noap = NULL;
        TAILQ_INIT(&w->menus);
        TAILQ_INSERT_TAIL(&dhcp->wi_scans, w, next);
    } else {
        txt = NULL;
        msg = N_("New Access Point");
        for (s1 = scans; s1; s1 = s1->next) {
            for (s2 = w->scans; s2; s2 = s2->next)
                if (g_strcmp0(s1->ssid, s2->ssid) == 0)
                    break;
            if (s2 == NULL) {
                if (txt == NULL)
                    txt = g_strdup(s1->ssid);
                else {
                    msg = N_("New Access Points");
                    t = g_strconcat(txt, "\n",
                        s1->ssid, NULL);
                    g_free(txt);
                    txt = t;
                }
            }
        }
        if (txt) {
            notify(msg, txt, "network-wireless");
            g_free(txt);
        }
        menu_update_scans(w, scans, dhcp->plugin);
    }

    if (!dhcp->ani_timer) {
        s1 = get_strongest_scan(dhcp->plugin);
        if (s1)
            msg = get_strength_icon_name(s1->strength.value);
        else if (dhcp->online)
            msg = "network-transmit-receive";
        else
            msg = "network-offline";
        set_icon (dhcp->panel, dhcp->tray_icon, msg, 0);
    }
}

static void
dhcpcd_wpa_status_cb(DHCPCD_WPA *wpa,
    unsigned int status, const char *status_msg, gpointer p)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) p;
    DHCPCD_IF *i;
    WI_SCAN *w, *wn;

    i = dhcpcd_wpa_if(wpa);
    g_message(_("%s: WPA status %s"), i->ifname, status_msg);
    if (status == DHC_DOWN) {
        dhcpcd_unwatch(-1, wpa, &dhcp->watches);
        TAILQ_FOREACH_SAFE(w, &dhcp->wi_scans, next, wn) {
            if (w->interface == i) {
                TAILQ_REMOVE(&dhcp->wi_scans, w, next);
                menu_remove_if(w, dhcp);
                dhcpcd_wi_scans_free(w->scans);
                g_free(w);
            }
        }
    }
}

static gboolean
bgscan(gpointer data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) data;
    WI_SCAN *w;
    DHCPCD_WPA *wpa;

    TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
        if (dhcpcd_is_wireless(w->interface)) {
            wpa = dhcpcd_wpa_find(dhcp->con, w->interface->ifname);
            if (wpa &&
                (!w->interface->up ||
                dhcpcd_wpa_can_background_scan(wpa)))
                dhcpcd_wpa_scan(wpa);
        }
    }

    return TRUE;
}

static gboolean dhcpcdui_button_press_event (GtkWidget *widget, GdkEventButton *event, LXPanel *panel)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data (widget);

    /* Show or hide the popup menu on left-click */
    if (event->button == 1)
    {
        menu_show (dhcp);
        return TRUE;
    }
    else return FALSE;
}

static GtkWidget *dhcpcdui_configure (LXPanel *panel, GtkWidget *p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data (p);

    return prefs_show (dhcp);
}

static void dhcpcdui_configuration_changed (LXPanel *panel, GtkWidget *p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data (p);
    const char *icon;
    DHCPCD_WI_SCAN *s1;

    if (!dhcp->ani_timer)
    {
        s1 = get_strongest_scan (p);
        if (s1)
            icon = get_strength_icon_name (s1->strength.value);
        else if (dhcp->online)
            icon = "network-transmit-receive";
        else
            icon = "network-offline";
        set_icon (dhcp->panel, dhcp->tray_icon, icon, 0);
    }
}

/* Plugin destructor. */
static void dhcpcdui_destructor (gpointer user_data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) user_data;

    /* Close associated dialogs and menu */
    wpa_abort (dhcp);
    menu_abort (dhcp);
    prefs_abort (dhcp);

    /* Close connection and kill timers which would reopen it */
    dhcpcd_close(dhcp->con);
    if (dhcp->reopen_timer != 0) g_source_remove (dhcp->reopen_timer);
    if (dhcp->wpa_reopen_timer != 0) g_source_remove(dhcp->wpa_reopen_timer);

    /* Remove other timers */
    if (dhcp->bgscan_timer != 0) g_source_remove (dhcp->bgscan_timer);
    if (dhcp->defscan_timer != 0) g_source_remove (dhcp->defscan_timer);
    if (dhcp->ani_timer != 0) g_source_remove (dhcp->ani_timer);

    /* Deallocate memory */
    dhcpcd_free (dhcp->con);
    g_free (dhcp);
}

/* Plugin constructor. */
static GtkWidget *dhcpcdui_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context */
    DHCPCDUIPlugin * dhcp = g_new0 (DHCPCDUIPlugin, 1);
    GtkWidget *p;

    setlocale (LC_ALL, "");
    bindtextdomain (PACKAGE, NULL);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    dhcp->tray_icon = gtk_image_new ();
    set_icon (panel, dhcp->tray_icon, "network-offline", 0);
    gtk_widget_set_tooltip_text (dhcp->tray_icon, _("Connecting to dhcpcd ..."));
    gtk_widget_set_visible (dhcp->tray_icon, true);

    dhcp->online = false;

    TAILQ_INIT(&dhcp->wi_scans);
    g_message(_("Connecting ..."));
    dhcp->con = dhcpcd_new ();
    if (dhcp->con ==  NULL)
    {
        g_critical ("libdhcpcd: %s", strerror(errno));
        return NULL;
    }
    dhcpcd_set_progname (dhcp->con, "dhcpcd-gtk");

    /* Allocate top level widget and set into Plugin widget pointer. */
    dhcp->panel = panel;
    dhcp->plugin = p = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (dhcp->plugin), GTK_RELIEF_NONE);
    g_signal_connect (dhcp->plugin, "button-press-event", G_CALLBACK(dhcpcdui_button_press_event), NULL);
    dhcp->settings = settings;
    lxpanel_plugin_set_data (p, dhcp, dhcpcdui_destructor);
    gtk_widget_add_events (p, GDK_BUTTON_PRESS_MASK);

    /* Allocate icon as a child of top level */
    gtk_container_add (GTK_CONTAINER(p), dhcp->tray_icon);

    /* Setup callbacks */
    dhcpcd_set_status_callback (dhcp->con, dhcpcd_status_cb, dhcp);
    dhcpcd_set_if_callback (dhcp->con, dhcpcd_if_cb, dhcp);
    dhcpcd_wpa_set_scan_callback (dhcp->con, dhcpcd_wpa_scan_cb, dhcp);
    dhcpcd_wpa_set_status_callback (dhcp->con, dhcpcd_wpa_status_cb, dhcp);
    if (dhcpcd_try_open (dhcp))
        dhcp->reopen_timer = g_timeout_add (DHCPCD_RETRYOPEN, dhcpcd_try_open, dhcp);

    /* Start background scanning */
    dhcp->defscan_timer = g_timeout_add (DHCPCD_WPA_SCAN_LONG, bgscan, dhcp);

    /* Show the widget, and return. */
    gtk_widget_show_all (p);
    return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, dhcpcdui)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Wireless & Wired Network"),
    .description = N_("Control for dhcpcd network interface"),
    .new_instance = dhcpcdui_constructor,
    .config = dhcpcdui_configure,
    .reconfigure = dhcpcdui_configuration_changed,
    .button_press_event = dhcpcdui_button_press_event
};
