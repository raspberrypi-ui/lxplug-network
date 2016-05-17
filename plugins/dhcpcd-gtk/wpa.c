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

#include "dhcpcd-gtk.h"

//static GtkWidget *wpa_dialog, *wpa_err;

static void
wpa_show_err(const char *title, const char *txt, DHCPCDUIPlugin *dhcp)
{

    if (dhcp->wpa_err)
        gtk_widget_destroy(dhcp->wpa_err);
    dhcp->wpa_err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", txt);
    gtk_window_set_title(GTK_WINDOW(dhcp->wpa_err), title);
    gtk_dialog_run(GTK_DIALOG(dhcp->wpa_err));
    gtk_widget_destroy(dhcp->wpa_err);
}

static void
onEnter(_unused GtkWidget *widget, gpointer *data)
{

    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
}

void
wpa_abort(DHCPCDUIPlugin *dhcp)
{

    if (dhcp->wpa_err) {
        gtk_widget_destroy(dhcp->wpa_err);
        dhcp->wpa_err = NULL;
    }
    if (dhcp->wpa_dialog) {
        gtk_widget_destroy(dhcp->wpa_dialog);
        dhcp->wpa_dialog = NULL;
    }
}

static bool
wpa_conf(int werr, DHCPCDUIPlugin *dhcp)
{
    const char *errt;

    switch (werr) {
    case DHCPCD_WPA_SUCCESS:
        return true;
    case DHCPCD_WPA_ERR_DISCONN:
        errt = _("Failed to disconnect.");
        break;
    case DHCPCD_WPA_ERR_RECONF:
        errt = _("Failed to reconfigure.");
        break;
    case DHCPCD_WPA_ERR_SET:
        errt = _("Failed to set key management.");
        break;
    case DHCPCD_WPA_ERR_SET_PSK:
        errt = _("Failed to set password, probably too short.");
        break;
    case DHCPCD_WPA_ERR_ENABLE:
        errt = _("Failed to enable the network.");
        break;
    case DHCPCD_WPA_ERR_SELECT:
        errt = _("Failed to select the network.");
        break;
    case DHCPCD_WPA_ERR_ASSOC:
        errt = _("Failed to start association.");
        break;
    case DHCPCD_WPA_ERR_WRITE:
        errt =_("Failed to save wpa_supplicant configuration.\n\nYou should add update_config=1 to /etc/wpa_supplicant.conf.");
        break;
    default:
        errt = strerror(errno);
        break;
    }
    wpa_show_err(_("Error enabling network"), errt, dhcp);
    return false;
}

bool wpa_disconnect (DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *scan)
{
	DHCPCDUIPlugin *dhcp = (DHCPCDUIPlugin *) dhcpcd_wpa_get_context (wpa);
    int id = dhcpcd_wpa_network_find_new (wpa, scan->ssid);
    if (id == -1)
    {
        wpa_show_err (_("Error disconnecting network"), _("Could not find SSID to disconnect"), dhcp);
        return false;
    }
	if (!dhcpcd_wpa_network_remove (wpa, id))
    {
        wpa_show_err (_("Error disconnecting network"), _("Could not remove network"), dhcp);
        return false;
    }
	if (!dhcpcd_wpa_config_write(wpa))
    {
        wpa_show_err (_("Error disconnecting network"), _("Could not write configuration"), dhcp);
        return false;
    }
	return true;
}

bool
wpa_configure(DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *scan)
{
    DHCPCD_WI_SCAN s;
    DHCPCDUIPlugin *dhcp = (DHCPCDUIPlugin *) dhcpcd_wpa_get_context (wpa);
    GtkWidget *label, *psk, *vbox, *hbox;
    const char *var;
    int result;
    bool retval;

    /* Take a copy of scan incase it's destroyed by a scan update */
    memcpy(&s, scan, sizeof(s));
    s.next = NULL;

    if (!(s.flags & WSF_PSK))
        return wpa_conf(dhcpcd_wpa_configure(wpa, &s, NULL), dhcp);

    if (dhcp->wpa_dialog)
        gtk_widget_destroy(dhcp->wpa_dialog);

    dhcp->wpa_dialog = gtk_dialog_new_with_buttons(s.ssid,
        NULL,
        GTK_DIALOG_MODAL,
        GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_position(GTK_WINDOW(dhcp->wpa_dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_resizable(GTK_WINDOW(dhcp->wpa_dialog), false);
    gtk_window_set_icon_name(GTK_WINDOW(dhcp->wpa_dialog),
        "network-wireless-encrypted");
    gtk_dialog_set_default_response(GTK_DIALOG(dhcp->wpa_dialog),
        GTK_RESPONSE_ACCEPT);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(dhcp->wpa_dialog));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new(_("Pre Shared Key:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, false, false, 5);
    psk = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(psk), 130);
    g_signal_connect(G_OBJECT(psk), "activate",
        G_CALLBACK(onEnter), dhcp->wpa_dialog);
    gtk_box_pack_start(GTK_BOX(hbox), psk, true, true, 5);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    gtk_widget_show_all(dhcp->wpa_dialog);
    result = gtk_dialog_run(GTK_DIALOG(dhcp->wpa_dialog));

    retval = false;
    if (result == GTK_RESPONSE_ACCEPT) {
        var = gtk_entry_get_text(GTK_ENTRY(psk));
        if (*var == '\0')
            retval = wpa_conf(dhcpcd_wpa_select(wpa, &s), dhcp);
        else
            retval = wpa_conf(dhcpcd_wpa_configure(wpa, &s, var), dhcp);
    }
    if (dhcp->wpa_dialog) {
        gtk_widget_destroy(dhcp->wpa_dialog);
        dhcp->wpa_dialog = NULL;
    }
    return retval;
}
