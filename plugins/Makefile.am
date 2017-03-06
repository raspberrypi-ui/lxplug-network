## Process this file with automake to produce Makefile.in

## loadable plugins
if BUILD_PLUGIN_LOADER
DYNAMIC_PLUGINS = \
	$(PLUGINS_LIST)
endif

pkglib_LTLIBRARIES = $(DYNAMIC_PLUGINS)

EXTRA_LTLIBRARIES = \
	dhcpcdui.la

pkglibdir = $(libdir)/lxpanel/plugins

# dhcpcdui
dhcpcdui_la_SOURCES = \
	dhcpcd-gtk/main.c \
	dhcpcd-gtk/menu.c \
	dhcpcd-gtk/prefs.c \
	dhcpcd-gtk/wpa.c \
	libdhcpcd/dhcpcd.c \
	libdhcpcd/config.c \
	libdhcpcd/wpa.c

dhcpcdui_la_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_DATA_DIR=\""$(datadir)/lxpanel"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I/usr/include/lxpanel \
	-I$(srcdir)/dhcpcd-gtk \
	-I$(srcdir)/libdhcpcd \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

dhcpcdui_la_LDFLAGS = \
	$(PACKAGE_LIBS) \
	-module @LXPANEL_MODULE@

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/lxpanel/plugins/*.la
	rm -f $(DESTDIR)$(libdir)/lxpanel/plugins/*.a
	rm -f $(DESTDIR)$(libdir)/lxpanel/plugins/*.

if BUILD_PLUGIN_LOADER
DYNAMIC_PLUGINS_INSTALLED = $(DYNAMIC_PLUGINS:.la=.so)
uninstall-hook:
	cd $(DESTDIR)$(libdir)/lxpanel/plugins && rm -f $(DYNAMIC_PLUGINS_INSTALLED) || true
endif
