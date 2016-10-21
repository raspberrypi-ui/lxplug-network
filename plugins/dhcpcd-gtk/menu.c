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

#include "config.h"
#include "dhcpcd-gtk.h"

#if 0
static const char *copyright = "Copyright (c) 2009-2015 Roy Marples";

static GtkStatusIcon *sicon;
static GtkWidget *menu;
static GtkAboutDialog *about;
static guint bgscan_timer;

static void
on_pref(_unused GObject *o, gpointer data)
{

    prefs_show((DHCPCD_CONNECTION *)data);
}

static void
on_quit(void)
{

    wpa_abort();
    gtk_main_quit();
}
#endif

static WI_SCAN *
wi_scan_find(DHCPCD_WI_SCAN *scan, GtkWidget *p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data(p);
    WI_SCAN *w;
    DHCPCD_WI_SCAN *dw;

    TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
        for (dw = w->scans; dw; dw = dw->next)
            if (dw == scan)
                return w;
    }
    return NULL;
}

static void disconnect_prompt (DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *scan)
{
    GtkWidget *dlg, *lbl;
    char buffer[256];
    int res;

    sprintf (buffer, _("Do you want to disconnect from the Wi-Fi network '%s'?"), scan->ssid);
    dlg = gtk_dialog_new_with_buttons (_("Disconnect Wi-Fi Network"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, 0, GTK_STOCK_OK, 1, NULL);
    lbl = gtk_label_new (buffer);
    gtk_label_set_line_wrap (GTK_LABEL (lbl), TRUE);
    gtk_label_set_justify (GTK_LABEL (lbl), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), lbl , TRUE, TRUE, 0);
    gtk_widget_show_all (dlg);

    // block while waiting for user response
    res = gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
    if (res) wpa_disconnect(wpa, scan);
}

static void
ssid_hook(GtkMenuItem *item, GtkWidget *p)
{
    DHCPCD_WI_SCAN *scan;
    WI_SCAN *wi;

    scan = g_object_get_data(G_OBJECT(item), "dhcpcd_wi_scan");
    wi = wi_scan_find(scan, p);
    if (wi) {
        DHCPCD_CONNECTION *con;

        con = dhcpcd_if_connection(wi->interface);
        if (con) {
            DHCPCD_WPA *wpa;

            wpa = dhcpcd_wpa_find(con, wi->interface->ifname);
            if (wpa)
            {
                if (dhcpcd_wi_associated(wi->interface, scan))
                    disconnect_prompt (wpa, scan);
                else
                    wpa_configure(wpa, scan);
            }
        }
    }
}
#if 0
static void
on_about(_unused GtkMenuItem *item)
{

    if (about == NULL) {
        about = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
        gtk_about_dialog_set_version(about, VERSION);
        gtk_about_dialog_set_copyright(about, copyright);
        gtk_about_dialog_set_website_label(about, "dhcpcd Website");
        gtk_about_dialog_set_website(about,
            "http://roy.marples.name/projects/dhcpcd");
        gtk_about_dialog_set_logo_icon_name(about,
            "network-transmit-receive");
        gtk_about_dialog_set_comments(about,
            "Part of the dhcpcd project");
    }
    gtk_window_set_position(GTK_WINDOW(about), GTK_WIN_POS_MOUSE);
    gtk_window_present(GTK_WINDOW(about));
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_hide(GTK_WIDGET(about));
}
#endif
static bool
is_associated(WI_SCAN *wi, DHCPCD_WI_SCAN *scan)
{

    return dhcpcd_wi_associated(wi->interface, scan);
}

static bool
get_security_icon(int flags, const char **icon)
{
	bool active;

	active = true;
	if (flags & WSF_SECURE) {
		if (flags & WSF_PSK)
			*icon = "network-wireless-encrypted";
		else {
			*icon = "network-error";
			active = false;
		}
	} else
		*icon = "";
	return active;
}

static void
update_item(WI_SCAN *wi, WI_MENU *m, DHCPCD_WI_SCAN *scan, DHCPCDUIPlugin *dhcp)
{
    const char *icon;
    GtkWidget *sel = gtk_image_new ();

    m->scan = scan;

    g_object_set_data(G_OBJECT(m->menu), "dhcpcd_wi_scan", scan);

    m->associated = is_associated(wi, scan);
    if (m->associated) set_icon (dhcp->panel, sel, "dialog-ok-apply", 16);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(m->menu), sel);

    gtk_label_set_text (GTK_LABEL(m->ssid), scan->ssid);

    //m->icon = gtk_image_new ();
    //if (scan->flags & WSF_SECURE) set_icon (dhcp->panel, m->icon, "network-wireless-encrypted", 16);

    //m->strength = gtk_image_new ();
    //set_icon (dhcp->panel, m->strength, get_strength_icon_name (scan->strength.value), 16);

#if 0
    if (scan->wpa_flags[0] == '\0')
        gtk_widget_set_tooltip_text(m->menu, scan->bssid);
    else {
        char *tip = g_strconcat(scan->bssid, " ", scan->wpa_flags,
            NULL);
        gtk_widget_set_tooltip_text(m->menu, tip);
        g_free(tip);
    }
#endif

    g_object_set_data(G_OBJECT(m->menu), "dhcpcd_wi_scan", scan);
}

static WI_MENU *
create_menu(WI_SCAN *wis, DHCPCD_WI_SCAN *scan, GtkWidget *p)
{
    DHCPCDUIPlugin * dhcp = lxpanel_plugin_get_data(p);
    WI_MENU *wim;
    GtkWidget *box;
    const char *icon;
    bool active;

    wim = g_malloc(sizeof(*wim));
    wim->scan = scan;
    wim->menu = gtk_image_menu_item_new();
    gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (wim->menu), TRUE);
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_add(GTK_CONTAINER(wim->menu), box);

    wim->ssid = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(wim->ssid), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(box), wim->ssid, TRUE, TRUE, 0);

    wim->icon = gtk_image_new ();
    active = get_security_icon (scan->flags, &icon);
    set_icon (dhcp->panel, wim->icon, icon, 16);
    gtk_box_pack_start(GTK_BOX(box), wim->icon, FALSE, FALSE, 0);

    wim->strength = gtk_image_new ();
    set_icon (dhcp->panel, wim->strength, get_strength_icon_name (scan->strength.value), 16);
    gtk_box_pack_start(GTK_BOX(box), wim->strength, FALSE, FALSE, 0);

#if 0
    if (scan->wpa_flags[0] == '\0')
        gtk_widget_set_tooltip_text(wim->menu, scan->bssid);
    else {
        char *tip;

        tip = g_strconcat(scan->bssid, " ", scan->wpa_flags, NULL);
        gtk_widget_set_tooltip_text(wim->menu, tip);
        g_free(tip);
    }
#endif
    update_item(wis, wim, scan, dhcp);

    if (gtk_widget_get_sensitive (wim->menu) != active)
        gtk_widget_set_sensitive (wim->menu, active);

    g_signal_connect(G_OBJECT(wim->menu), "activate",
        G_CALLBACK(ssid_hook), p);
    g_object_set_data(G_OBJECT(wim->menu), "dhcpcd_wi_scan", scan);

    return wim;
}

void
menu_update_scans(WI_SCAN *wi, DHCPCD_WI_SCAN *scans, GtkWidget *p)
{
    WI_MENU *wim, *win;
    DHCPCD_WI_SCAN *s;
    gboolean separate = FALSE;

    if (wi->ifmenu == NULL) {
        dhcpcd_wi_scans_free(wi->scans);
        wi->scans = scans;
        return;
    }

    // clear down the menu
    TAILQ_FOREACH_SAFE (wim, &wi->menus, next, win)
    {
        TAILQ_REMOVE (&wi->menus, wim, next);
        gtk_widget_destroy (wim->menu);
        g_free (wim);
    }
    if (wi->sep) gtk_widget_destroy (wi->sep);
    wi->sep = NULL;
    if (wi->noap) gtk_widget_destroy (wi->noap);
    wi->noap = NULL;

    // loop through all scans, locating any associated AP
    for (s = scans; s; s = s->next)
    {
        if (is_associated (wi, s))
        {
            wim = create_menu (wi, s, p);
            TAILQ_INSERT_TAIL (&wi->menus, wim, next);
            gtk_menu_shell_append (GTK_MENU_SHELL (wi->ifmenu), wim->menu);
            gtk_widget_show_all (wim->menu);
            separate = TRUE;
        }
    }

    // loop through all scans, adding all unassociated APs
    for (s = scans; s; s = s->next)
    {
        if (!is_associated (wi, s))
        {
            wim = create_menu (wi, s, p);
            TAILQ_INSERT_TAIL (&wi->menus, wim, next);
            if (separate)
            {
                wi->sep = gtk_separator_menu_item_new ();
                gtk_widget_show (wi->sep);
                gtk_menu_shell_append (GTK_MENU_SHELL (wi->ifmenu), wi->sep);
                separate = FALSE;
            }
            gtk_menu_shell_append (GTK_MENU_SHELL (wi->ifmenu), wim->menu);
            gtk_widget_show_all (wim->menu);
        }
    }

    dhcpcd_wi_scans_free(wi->scans);
    wi->scans = scans;

    if (gtk_widget_get_visible(wi->ifmenu))
        gtk_menu_reposition(GTK_MENU(wi->ifmenu));
}

void
menu_remove_if(WI_SCAN *wi, DHCPCDUIPlugin * dhcp)
{
    WI_MENU *wim;

    if (wi->ifmenu == NULL)
        return;

    if (wi->ifmenu == dhcp->menu)
    {
        dhcp->menu = NULL;
        gtk_widget_destroy(wi->ifmenu);
    }
    else
    {
        /* if there are multiple interfaces and hence a top-level menu, remove the entry for the removed interface */
        GList *children = gtk_container_get_children (GTK_CONTAINER(dhcp->menu));
        while ((children = g_list_next(children)) != NULL)
        {
            GtkWidget *item = children->data;
            if (!strcmp (gtk_menu_item_get_label (GTK_MENU_ITEM(item)), wi->interface->ifname))
                gtk_widget_destroy (GTK_WIDGET(item));
        }
    }
    wi->ifmenu = NULL;
    while ((wim = TAILQ_FIRST(&wi->menus))) {
        TAILQ_REMOVE(&wi->menus, wim, next);
        g_free(wim);
    }

    if (dhcp->menu && gtk_widget_get_visible(dhcp->menu))
        gtk_menu_reposition(GTK_MENU(dhcp->menu));
}

static GtkWidget *
add_scans(WI_SCAN *wi, GtkWidget *p)
{
    GtkWidget *m;
    DHCPCD_WI_SCAN *wis;
    WI_MENU *wim;
    gboolean separate = FALSE;

    wi->noap = NULL;

    if (wi->scans == NULL)
    {
        m = gtk_menu_new ();
        wi->noap = gtk_menu_item_new_with_label (_("No APs found - scanning..."));
        gtk_widget_set_sensitive (wi->noap, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), wi->noap);
        return m;
    }

    m = gtk_menu_new();

    wi->sep = NULL;
    for (wis = wi->scans; wis; wis = wis->next)
    {
        if (is_associated (wi, wis))
        {
            wim = create_menu (wi, wis, p);
            TAILQ_INSERT_TAIL (&wi->menus, wim, next);
            gtk_menu_shell_append (GTK_MENU_SHELL (m), wim->menu);
            separate = TRUE;
        }
    }
    for (wis = wi->scans; wis; wis = wis->next)
    {
        if (!is_associated (wi, wis))
        {
            wim = create_menu (wi, wis, p);
            TAILQ_INSERT_TAIL (&wi->menus, wim, next);
            if (separate)
            {
                wi->sep = gtk_separator_menu_item_new ();
                gtk_widget_show (wi->sep);
                gtk_menu_shell_append (GTK_MENU_SHELL (m), wi->sep);
                separate = FALSE;
            }
            gtk_menu_shell_append (GTK_MENU_SHELL (m), wim->menu);
        }
    }

    return m;
}

void
menu_abort(DHCPCDUIPlugin *data)
{
    WI_SCAN *wis;
    WI_MENU *wim;

    if (data->bgscan_timer) {
        g_source_remove(data->bgscan_timer);
        data->bgscan_timer = 0;
    }

    TAILQ_FOREACH(wis, &data->wi_scans, next) {
        wis->ifmenu = NULL;
        wis->sep = NULL;
        wis->noap = NULL;
        while ((wim = TAILQ_FIRST(&wis->menus))) {
            TAILQ_REMOVE(&wis->menus, wim, next);
            g_free(wim);
        }
    }

    if (data->menu != NULL) {
        gtk_widget_destroy(data->menu);
        g_object_ref_sink(data->menu);
        g_object_unref(data->menu);
        data->menu = NULL;
    }
}

static gboolean
menu_bgscan(gpointer data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) data;
    WI_SCAN *w;
    DHCPCD_WPA *wpa;

    if (dhcp->menu == NULL || !gtk_widget_get_visible(dhcp->menu)) {
        dhcp->bgscan_timer = 0;
        return FALSE;
    }

    TAILQ_FOREACH(w, &dhcp->wi_scans, next) {
        if (dhcpcd_is_wireless(w->interface)) {
            wpa = dhcpcd_wpa_find(dhcpcd_if_connection (w->interface), w->interface->ifname);
            if (wpa &&
                (!w->interface->up ||
                dhcpcd_wpa_can_background_scan(wpa)))
                dhcpcd_wpa_scan(wpa);
        }
    }

    return TRUE;
}

static void dhcpcdui_popup_set_position(GtkMenu * menu, gint * px, gint * py, gboolean * push_in, gpointer data)
{
    DHCPCDUIPlugin * dhcp = (DHCPCDUIPlugin *) data;

    /* Determine the coordinates. */
    lxpanel_plugin_popup_set_position_helper(dhcp->panel, dhcp->plugin, GTK_WIDGET(menu), px, py);
    *push_in = TRUE;
}

static int wifi_enabled (void)
{
    FILE *fp;

    // is rfkill installed?
    fp = popen ("test -e /usr/sbin/rfkill", "r");
    if (pclose (fp)) return -2;

    // is there wifi hardware that rfkill can see?
    fp = popen ("/usr/sbin/rfkill list wifi | grep -q blocked", "r");
    if (pclose (fp)) return -1;

    // is rfkill blocking wifi?
    fp = popen ("/usr/sbin/rfkill list wifi | grep -q 'Soft blocked: no'", "r");
    if (!pclose (fp)) return 1;
    return 0;
}

static void toggle_wifi (_unused GObject *o, _unused gpointer data)
{
    if (wifi_enabled ())
        system ("sudo /usr/sbin/rfkill block wifi");
    else
        system ("sudo /usr/sbin/rfkill unblock wifi");
}

void
menu_show (DHCPCDUIPlugin *data)
{
    WI_SCAN *w, *l;
    GtkWidget *item, *image;

    //sicon = icon;
    notify_close();
    prefs_abort(data);
    menu_abort(data);

    int wifi_state = wifi_enabled ();

    if (wifi_state == -1)
    {
        // rfkill is installed, but no hardware found
        data->menu = gtk_menu_new ();
        item = gtk_menu_item_new_with_label (_("No wireless interfaces found"));
        gtk_widget_set_sensitive (item, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (data->menu), item);
    }
    else if (wifi_state == 0)
    {
        // rfkill installed, h/w found, disabled
        data->menu = gtk_menu_new ();
        item = gtk_menu_item_new_with_label (_("Turn On Wi-Fi"));
        g_signal_connect (G_OBJECT(item), "activate", G_CALLBACK (toggle_wifi), NULL);
        gtk_menu_shell_append (GTK_MENU_SHELL (data->menu), item);
    }
    else
    {
    if ((w = TAILQ_FIRST(&data->wi_scans)) == NULL)
    {
        data->menu = gtk_menu_new ();
        item = gtk_menu_item_new_with_label (_("No wireless interfaces found"));
        gtk_widget_set_sensitive (item, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL(data->menu), item);
    }
    else
    {
    if ((l = TAILQ_LAST(&data->wi_scans, wi_scan_head)) && l != w) {
        data->menu = gtk_menu_new();
        TAILQ_FOREACH(w, &data->wi_scans, next) {
            item = gtk_image_menu_item_new_with_label(
                w->interface->ifname);
            gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);
            image = gtk_image_new ();
            set_icon (data->panel, image, "network-wireless", 16);
            gtk_image_menu_item_set_image(
                GTK_IMAGE_MENU_ITEM(item), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(data->menu), item);
            w->ifmenu = add_scans(w, data->plugin);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
                w->ifmenu);
        }
    } else {
        w->ifmenu = data->menu = add_scans(w, data->plugin);
    }

            if (wifi_state == 1)
            {
                // rfkill installed, h/w found, enabled
                item = gtk_separator_menu_item_new ();
                gtk_menu_shell_prepend (GTK_MENU_SHELL (data->menu), item);
                item = gtk_menu_item_new_with_label (_("Turn Off Wi-Fi"));
                g_signal_connect (G_OBJECT(item), "activate", G_CALLBACK (toggle_wifi), NULL);
                gtk_menu_shell_prepend (GTK_MENU_SHELL (data->menu), item);
            }
        }
    }

    if (data->menu) {
        gtk_widget_show_all(GTK_WIDGET(data->menu));
        gtk_menu_popup(GTK_MENU(data->menu), NULL, NULL,
            dhcpcdui_popup_set_position, data,
            1, gtk_get_current_event_time());

#ifdef BG_SCAN
        data->bgscan_timer = g_timeout_add(DHCPCD_WPA_SCAN_SHORT,
            menu_bgscan, data);
#endif
    }
}


#if 0
static void
on_activate(GtkStatusIcon *icon)
{
    WI_SCAN *w, *l;
    GtkWidget *item, *image;

    sicon = icon;
    notify_close();
    prefs_abort();
    menu_abort();

    if ((w = TAILQ_FIRST(&wi_scans)) == NULL)
        return;

    if ((l = TAILQ_LAST(&wi_scans, wi_scan_head)) && l != w) {
        menu = gtk_menu_new();
        TAILQ_FOREACH(l, &wi_scans, next) {
            item = gtk_image_menu_item_new_with_label(
                l->interface->ifname);
            gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);
            image = gtk_image_new_from_icon_name(
                "network-wireless", GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image(
                GTK_IMAGE_MENU_ITEM(item), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
            l->ifmenu = add_scans(l);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
                l->ifmenu);
        }
    } else {
        w->ifmenu = menu = add_scans(w);
    }

    if (menu) {
        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
            gtk_status_icon_position_menu, icon,
            1, gtk_get_current_event_time());

        bgscan_timer = g_timeout_add(DHCPCD_WPA_SCAN_SHORT,
            menu_bgscan, NULL);
    }
}

static void
on_popup(GtkStatusIcon *icon, guint button, guint32 atime, gpointer data)
{
    DHCPCD_CONNECTION *con;
    GtkMenu *mnu;
    GtkWidget *item, *image;

    notify_close();

    con = (DHCPCD_CONNECTION *)data;
    mnu = (GtkMenu *)gtk_menu_new();

    item = gtk_image_menu_item_new_with_mnemonic(_("_Preferences"));
    image = gtk_image_new_from_icon_name("preferences-system-network",
        GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    if (dhcpcd_status(con, NULL) == DHC_DOWN)
        gtk_widget_set_sensitive(item, false);
    else
        g_signal_connect(G_OBJECT(item), "activate",
            G_CALLBACK(on_pref), data);
    gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

    item = gtk_image_menu_item_new_with_mnemonic(_("_About"));
    image = gtk_image_new_from_icon_name("help-about",
        GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(on_about), icon);
    gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

    item = gtk_image_menu_item_new_with_mnemonic(_("_Quit"));
    image = gtk_image_new_from_icon_name("application-exit",
        GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(on_quit), icon);
    gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

    gtk_widget_show_all(GTK_WIDGET(mnu));
    gtk_menu_popup(GTK_MENU(mnu), NULL, NULL,
        gtk_status_icon_position_menu, icon, button, atime);
    if (button == 0)
        gtk_menu_shell_select_first(GTK_MENU_SHELL(mnu), FALSE);
}

void
menu_init(GtkStatusIcon *icon, DHCPCD_CONNECTION *con)
{

    g_signal_connect(G_OBJECT(icon), "activate",
        G_CALLBACK(on_activate), con);
    g_signal_connect(G_OBJECT(icon), "popup_menu",
        G_CALLBACK(on_popup), con);
}
#endif


#if GTK_MAJOR_VERSION == 2
GtkWidget *
gtk_box_new(GtkOrientation o, gint s)
{

    if (o == GTK_ORIENTATION_HORIZONTAL)
        return gtk_hbox_new(false, s);
    else
        return gtk_vbox_new(false, s);
}
#endif
