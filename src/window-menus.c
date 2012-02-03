/*
An implementation of indicator object showing menus from applications.

Copyright 2010 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include <libdbusmenu-gtk/menu.h>
#include <glib.h>
#include <gio/gio.h>

#include "window-menus.h"
#include "indicator-appmenu-marshal.h"

/* Private parts */

typedef struct _WindowMenusPrivate WindowMenusPrivate;
struct _WindowMenusPrivate {
	guint windowid;
	DbusmenuGtkMenu * menu;
	DbusmenuGtkClient * client;
	DbusmenuMenuitem * root;
	GCancellable * props_cancel;
	GDBusProxy * props;
	gboolean error_state;
	guint   retry_timer;
};

#define WINDOW_MENUS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), WINDOW_MENUS_TYPE, WindowMenusPrivate))

/* Signals */

enum {
	ENTRY_ADDED,
	ENTRY_REMOVED,
	ERROR_STATE,
	STATUS_CHANGED,
	SHOW_MENU,
	A11Y_UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Prototypes */

static void window_menus_class_init (WindowMenusClass *klass);
static void window_menus_init       (WindowMenus *self);
static void window_menus_dispose    (GObject *object);
static void root_changed            (DbusmenuClient * client, DbusmenuMenuitem * new_root, gpointer user_data);
static void event_status            (DbusmenuClient * client, DbusmenuMenuitem * mi, gchar * event, GVariant * evdata, guint timestamp, GError * error, gpointer user_data);
static void item_activate           (DbusmenuClient * client, DbusmenuMenuitem * item, guint timestamp, gpointer user_data);
static void status_changed          (DbusmenuClient * client, GParamSpec * pspec, gpointer user_data);
static void props_cb (GObject * object, GAsyncResult * res, gpointer user_data);

G_DEFINE_TYPE (WindowMenus, window_menus, G_TYPE_OBJECT);

/* Build the one-time class */
static void
window_menus_class_init (WindowMenusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (WindowMenusPrivate));

	object_class->dispose = window_menus_dispose;

	/* Signals */
	signals[ERROR_STATE] =   g_signal_new(WINDOW_MENUS_SIGNAL_ERROR_STATE,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET (WindowMenusClass, error_state),
	                                      NULL, NULL,
	                                      g_cclosure_marshal_VOID__BOOLEAN,
	                                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN, G_TYPE_NONE);
	signals[STATUS_CHANGED] = g_signal_new(WINDOW_MENUS_SIGNAL_STATUS_CHANGED,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET (WindowMenusClass, status_changed),
	                                      NULL, NULL,
	                                      g_cclosure_marshal_VOID__INT,
	                                      G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_NONE);
	signals[SHOW_MENU] =     g_signal_new(WINDOW_MENUS_SIGNAL_SHOW_MENU,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET (WindowMenusClass, show_menu),
	                                      NULL, NULL,
	                                      _indicator_appmenu_marshal_VOID__POINTER_UINT,
	                                      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_NONE);
	signals[A11Y_UPDATE] =   g_signal_new(WINDOW_MENUS_SIGNAL_A11Y_UPDATE,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET (WindowMenusClass, a11y_update),
	                                      NULL, NULL,
	                                      _indicator_appmenu_marshal_VOID__POINTER,
	                                      G_TYPE_NONE, 1, G_TYPE_POINTER, G_TYPE_NONE);

	return;
}

/* Initialize the per-instance data */
static void
window_menus_init (WindowMenus *self)
{
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(self);

	priv->menu = NULL;
	priv->client = NULL;
	priv->props_cancel = NULL;
	priv->props = NULL;
	priv->root = NULL;
	priv->error_state = FALSE;

	return;
}

/* Destroy objects */
static void
window_menus_dispose (GObject *object)
{
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(object);

	if (priv->root != NULL) {
		root_changed(DBUSMENU_CLIENT(priv->client), NULL, object);
		g_warn_if_fail(priv->root == NULL);
	}

	if (priv->menu != NULL) {
		if (priv->client != NULL) {
			g_signal_handlers_disconnect_by_func(G_OBJECT(priv->client), G_CALLBACK(root_changed),    object);
			g_signal_handlers_disconnect_by_func(G_OBJECT(priv->client), G_CALLBACK(event_status),    object);
			g_signal_handlers_disconnect_by_func(G_OBJECT(priv->client), G_CALLBACK(item_activate),   object);
			g_signal_handlers_disconnect_by_func(G_OBJECT(priv->client), G_CALLBACK(status_changed),  object);
		}

		g_clear_object(&priv->menu);
		priv->client = NULL;
	}
	
	if (priv->props != NULL) {
		g_object_unref(G_OBJECT(priv->props));
		priv->props = NULL;
	}

	if (priv->props_cancel != NULL) {
		g_cancellable_cancel(priv->props_cancel);
		g_object_unref(priv->props_cancel);
		priv->props_cancel = NULL;
	}

	if (priv->retry_timer != 0) {
		g_source_remove(priv->retry_timer);
		priv->retry_timer = 0;
	}

	G_OBJECT_CLASS (window_menus_parent_class)->dispose (object);
	return;
}

/* Retry the event sending to the server to see if we can get things
   working again. */
static gboolean
retry_event (gpointer user_data)
{
	g_debug("Retrying event");
	g_return_val_if_fail(IS_WINDOW_MENUS(user_data), FALSE);
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(user_data);

	dbusmenu_menuitem_handle_event(dbusmenu_client_get_root(DBUSMENU_CLIENT(priv->client)),
	                               "x-appmenu-retry-ping",
	                               NULL,
	                               0);

	priv->retry_timer = 0;

	return FALSE;
}

/* Listen to whether our events are successfully sent */
static void
event_status (DbusmenuClient * client, DbusmenuMenuitem * mi, gchar * event, GVariant * evdata, guint timestamp, GError * error, gpointer user_data)
{
	g_return_if_fail(IS_WINDOW_MENUS(user_data));
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(user_data);

	/* We don't care about status where there are no errors
	   when we're in a happy state, just let them go. */
	if (error == NULL && priv->error_state == FALSE) {
		return;
	}

	/* Oh, things are working now! */
	if (error == NULL) {
		g_debug("Error state repaired");
		priv->error_state = FALSE;
		g_signal_emit(G_OBJECT(user_data), signals[ERROR_STATE], 0, priv->error_state, TRUE);

		GList * children = NULL;

		if (DBUSMENU_MENUITEM(priv->root)) {
			children = dbusmenu_menuitem_get_children(DBUSMENU_MENUITEM(priv->root));
		}

		GList * child = children;
		while (child != NULL) {
			if (DBUSMENU_MENUITEM(child->data)) {
				DbusmenuMenuitem * mi = DBUSMENU_MENUITEM(child->data);
				GtkMenuItem * gmi = dbusmenu_gtkclient_menuitem_get(priv->client, mi);

				if (gmi != NULL) {
					gtk_widget_set_sensitive(GTK_WIDGET(gmi), dbusmenu_menuitem_property_get_bool(mi, DBUSMENU_MENUITEM_PROP_ENABLED));
				}
			}
			child = g_list_next(child);
		}

		g_list_free(children);

		if (priv->retry_timer != 0) {
			g_source_remove(priv->retry_timer);
			priv->retry_timer = 0;
		}

		return;
	}

	/* Uhg, means that events are breaking, now we need to
	   try and handle that case. */
	priv->error_state = TRUE;
	g_signal_emit(G_OBJECT(user_data), signals[ERROR_STATE], 0, priv->error_state, TRUE);

	GList * children = NULL;

	if (GTK_IS_MENU(priv->menu)) {
		children = gtk_container_get_children(GTK_CONTAINER(priv->menu));
	}

	GList * child = children;
	while (child != NULL) {
		if (GTK_IS_MENU_ITEM(child->data)) {
			gtk_widget_set_sensitive(GTK_WIDGET(child->data), FALSE);
		}
		child = g_list_next(child);
	}

	g_list_free(children);

	if (priv->retry_timer == 0) {
		g_debug("Setting up retry timer");
		priv->retry_timer = g_timeout_add_seconds(1, retry_event, user_data);
	}

	return;
}

/* Called when a menu item wants to be displayed.  We need to see if
   it's one of our root items and pass it up if so. */
static void
item_activate (DbusmenuClient * client, DbusmenuMenuitem * item, guint timestamp, gpointer user_data)
{
	g_return_if_fail(IS_WINDOW_MENUS(user_data));
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(user_data);

	if (priv->root == NULL) {
		return;
	}

	/* TODO: 
	IndicatorObjectEntry * entry = get_entry(WINDOW_MENUS(user_data), item, NULL);
	if (entry == NULL) {
		return;
	}

	g_signal_emit(G_OBJECT(user_data), signals[SHOW_MENU], 0, entry, timestamp, TRUE);
	*/

	return;
}

/* Called when the client changes its status.  Used to show panel if requested.
   (Say, by an Alt press.) */
static void
status_changed (DbusmenuClient * client, GParamSpec * pspec, gpointer user_data)
{
	g_signal_emit(G_OBJECT(user_data), signals[STATUS_CHANGED], 0, dbusmenu_client_get_status (client));
}

DbusmenuStatus
window_menus_get_status (WindowMenus * wm)
{
	g_return_val_if_fail(IS_WINDOW_MENUS(wm), DBUSMENU_STATUS_NORMAL);
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);

	return dbusmenu_client_get_status (DBUSMENU_CLIENT (priv->client));
}

/* Build a new window menus object and attach to the signals to build
   up the representative menu. */
WindowMenus *
window_menus_new (const guint windowid, const gchar * dbus_addr, const gchar * dbus_object)
{
	g_debug("Creating new windows menu: %X, %s, %s", windowid, dbus_addr, dbus_object);

	g_return_val_if_fail(windowid != 0, NULL);
	g_return_val_if_fail(dbus_addr != NULL, NULL);
	g_return_val_if_fail(dbus_object != NULL, NULL);

	WindowMenus * newmenu = WINDOW_MENUS(g_object_new(WINDOW_MENUS_TYPE, NULL));
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(newmenu);

	priv->windowid = windowid;

	/* Build the service proxy */
	priv->props_cancel = g_cancellable_new();
	g_object_ref(newmenu); /* Take a ref for the async callback */
	g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
	                         G_DBUS_PROXY_FLAGS_NONE,
	                         NULL,
	                         dbus_addr,
	                         dbus_object,
	                         "org.freedesktop.DBus.Properties",
	                         priv->props_cancel,
	                         props_cb,
	                         newmenu);

	priv->menu = dbusmenu_gtkmenu_new((gchar *)dbus_addr, (gchar *)dbus_object);
	priv->client = dbusmenu_gtkmenu_get_client(priv->menu);
	GtkAccelGroup * agroup = gtk_accel_group_new();
	dbusmenu_gtkclient_set_accel_group(priv->client, agroup);
	g_object_unref(agroup);

	g_signal_connect(G_OBJECT(priv->client), DBUSMENU_GTKCLIENT_SIGNAL_ROOT_CHANGED, G_CALLBACK(root_changed),   newmenu);
	g_signal_connect(G_OBJECT(priv->client), DBUSMENU_CLIENT_SIGNAL_EVENT_RESULT, G_CALLBACK(event_status), newmenu);
	g_signal_connect(G_OBJECT(priv->client), DBUSMENU_CLIENT_SIGNAL_ITEM_ACTIVATE, G_CALLBACK(item_activate), newmenu);
	g_signal_connect(G_OBJECT(priv->client), "notify::" DBUSMENU_CLIENT_PROP_STATUS, G_CALLBACK(status_changed), newmenu);

	DbusmenuMenuitem * root = dbusmenu_client_get_root(DBUSMENU_CLIENT(priv->client));
	if (root != NULL) {
		root_changed(DBUSMENU_CLIENT(priv->client), root, newmenu);
	}

	return newmenu;
}

/* Callback from trying to create the proxy for the service, this
   could include starting the service. */
static void
props_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return; // Must exit before accessing freed memory
	}

	WindowMenus * self = WINDOW_MENUS(user_data);
	g_return_if_fail(self != NULL);

	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(self);

	if (priv->props_cancel != NULL) {
		g_object_unref(priv->props_cancel);
		priv->props_cancel = NULL;
	}

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for window %u: %s", priv->windowid, error->message);
		g_error_free(error);
		goto out;
	}

	/* Okay, we're good to grab the proxy at this point, we're
	sure that it's ours. */
	priv->props = proxy;

out:
	g_object_unref(G_OBJECT(self));

	return;
}

/* Respond to the root menu item on our client changing */
static void
root_changed (DbusmenuClient * client, DbusmenuMenuitem * new_root, gpointer user_data)
{
	g_return_if_fail(IS_WINDOW_MENUS(user_data));
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(user_data);

	if (priv->root != NULL) {
		g_object_unref(priv->root);
	}

	priv->root = new_root;

	/* See if we've got new root */
	if (new_root == NULL) {
		return;
	}

	g_object_ref(priv->root);

	return;
}

/* Get the XID of this window */
guint
window_menus_get_xid (WindowMenus * wm)
{
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);
	return priv->windowid;
}

/* Get the path for this object */
gchar *
window_menus_get_path (WindowMenus * wm)
{
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);
	GValue obj = {0};
	g_value_init(&obj, G_TYPE_STRING);
	g_object_get_property(G_OBJECT(priv->client), DBUSMENU_CLIENT_PROP_DBUS_OBJECT, &obj);
	gchar * retval = g_value_dup_string(&obj);
	g_value_unset(&obj);
	return retval;
}

/* Get the address of this object */
gchar *
window_menus_get_address (WindowMenus * wm)
{
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);
	GValue obj = {0};
	g_value_init(&obj, G_TYPE_STRING);
	g_object_get_property(G_OBJECT(priv->client), DBUSMENU_CLIENT_PROP_DBUS_NAME, &obj);
	gchar * retval = g_value_dup_string(&obj);
	g_value_unset(&obj);
	return retval;
}

/* Return whether we're in an error state or not */
gboolean
window_menus_get_error_state (WindowMenus * wm)
{
	g_return_val_if_fail(IS_WINDOW_MENUS(wm), TRUE);
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);
	return priv->error_state;
}

/* Signaled when the menu item is activated on the panel so we
   can pass it down the stack.

   Unfortunately, that causes is to look through the levels of
   indirection that we have here.  The first is the Entry to the
   GtkMenuItem and then we have to translate that into a
   DbusmenuMenuitem to send the actual signal.
*/
void
window_menus_entry_activate (WindowMenus * wm, IndicatorObjectEntry * entry, guint timestamp)
{
	g_return_if_fail(IS_WINDOW_MENUS(wm));
	g_return_if_fail(entry != NULL);
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);

	/* If the entry doesn't have a menu, it doesn't make sense to
	   about_to_show it.  So let's just stop things here */
	if (entry->menu == NULL) {
		return;
	}

	/* Look to find the GTK Menu Item we need */
	GtkMenuItem * gmi = NULL;
	GList * children = gtk_container_get_children(GTK_CONTAINER(priv->menu));
	GList * child = children;

	while (child != NULL) {
		if (GTK_IS_MENU_ITEM(child->data)) {
			GtkMenu * sub = GTK_MENU(gtk_menu_item_get_submenu(GTK_MENU_ITEM(child->data)));

			if (sub == entry->menu) {
				gmi = GTK_MENU_ITEM(child->data);
				break;
			}
		}
	
		child = g_list_next(child);
	}
	g_list_free(children);

	/* Find the DBusmenu menuitem we need (if we got one before) */
	DbusmenuMenuitem * mi = NULL;
	children = dbusmenu_menuitem_get_children(priv->root);
	child = children;

	while (child != NULL && gmi != NULL) {
		GtkMenuItem * lgmi = NULL;

		if (DBUSMENU_IS_MENUITEM(child->data)) {
			lgmi = dbusmenu_gtkclient_menuitem_get(priv->client, DBUSMENU_MENUITEM(child->data));
		}

		if (lgmi == gmi) {
			mi = DBUSMENU_MENUITEM(child->data);
			break;
		}
	}
	g_list_free(children);

	/* Send About To Show to the menu item */
	if (mi != NULL) {
		dbusmenu_menuitem_send_about_to_show(mi, NULL, NULL);
	}
}

/* Return our menu (which is actually a DbusmenuGtkMenu) to the caller
   as its superclass, a GtkMenu */
GtkMenu *
window_menus_get_menu (WindowMenus * wm)
{
	g_return_val_if_fail(IS_WINDOW_MENUS(wm), NULL);
	WindowMenusPrivate * priv = WINDOW_MENUS_GET_PRIVATE(wm);

	return GTK_MENU(priv->menu);
}
