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

static void clear_dlg (GtkButton *button, gpointer user_data)
{
    gtk_widget_destroy (GTK_WIDGET (user_data));
}

static void
wpa_show_err(const char *title, const char *txt, DHCPCDUIPlugin *dhcp)
{
    GtkBuilder *builder;
    char *buffer;

    if (dhcp->wpa_err) gtk_widget_destroy (dhcp->wpa_err);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/lxpanel-modal.ui");
    dhcp->wpa_err = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    buffer = g_strdup_printf ("%s - %s", title, txt);
    gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "modal_msg")), buffer);
    g_signal_connect (gtk_builder_get_object (builder, "modal_ok"), "clicked", G_CALLBACK (clear_dlg), dhcp->wpa_err);
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "modal_cancel")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "modal_pb")));
    g_object_unref (builder);

    gtk_widget_show (dhcp->wpa_err);
    g_free (buffer);
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

// parse the wpa_supplicant data file for an existing PSK
static char *find_psk_for_network (char *ssid)
{
    FILE *fp;
    char *line = NULL, *seek, *res, *ret = NULL;
    size_t len = 0;
    int state = 0;

    seek = g_strdup_printf ("ssid=\"%s\"", ssid);
    fp = fopen ("/etc/wpa_supplicant/wpa_supplicant.conf", "rb");
    if (fp)
    {
        while (getline (&line, &len, fp) > 0)
        {
            // state : 1 in a network block; 2 in network block with matching ssid; 0 otherwise
            if (strstr (line, "network={")) state = 1;
            else if (strstr (line, "}")) state = 0;
            else if (state)
            {
                if (strstr (line, seek)) state = 2;
                else if (state == 2 && (res = strstr (line, "psk=")))
                {
                    if (!strchr (strtok (line, "\""), '#'))
                        ret = g_strdup (strtok (NULL, "\""));
                    break;
                }
            }
        }
        g_free (line);
        fclose (fp);
    }
    g_free (seek);
    return ret;
}

static void psk_toggle (GtkButton *btn, gpointer ptr)
{
    gtk_entry_set_visibility (GTK_ENTRY (ptr),
        !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)));
}

bool
wpa_configure(DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *scan)
{
    DHCPCD_WI_SCAN s;
    DHCPCDUIPlugin *dhcp = (DHCPCDUIPlugin *) dhcpcd_wpa_get_context (wpa);
    GtkWidget *label, *psk, *vbox, *hbox, *check;
    const char *var;
    int result;
    bool retval;
    char *epsk;

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
#if GTK_CHECK_VERSION(3, 0, 0)
        _("_Cancel"), GTK_RESPONSE_REJECT,
        _("_OK"), GTK_RESPONSE_ACCEPT,
#else
        GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
#endif
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
    if (epsk = find_psk_for_network (s.ssid))
    {
        gtk_entry_set_text (GTK_ENTRY(psk), epsk);
        g_free (epsk);
    }
    g_signal_connect(G_OBJECT(psk), "activate",
        G_CALLBACK(onEnter), dhcp->wpa_dialog);
    gtk_box_pack_start(GTK_BOX(hbox), psk, true, true, 5);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    check = gtk_check_button_new_with_label  (_("Hide characters"));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
    g_signal_connect (check, "toggled", G_CALLBACK (psk_toggle), psk);
    gtk_box_pack_end (GTK_BOX (hbox), check, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_entry_set_visibility (GTK_ENTRY (psk), FALSE);

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
