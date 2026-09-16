/*
 * Status Icons Toggle - quick setting tile
 * Calls ~/toggle-status-icons.sh when toggled
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <phosh-plugin.h>
#include "quick-setting.h"

#define PLUGIN_NAME "status-icons-toggle"
#define VISIBLE_FLAG ".config/phosh-indicators-visible"

typedef struct _StatusIconsToggle {
  PhoshQuickSetting parent;
  PhoshStatusIcon *status_icon;
} StatusIconsToggle;

typedef struct _StatusIconsToggleClass {
  PhoshQuickSettingClass parent_class;
} StatusIconsToggleClass;

#define TYPE_STATUS_ICONS_TOGGLE status_icons_toggle_get_type ()
#define STATUS_ICONS_TOGGLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_STATUS_ICONS_TOGGLE, StatusIconsToggle))
G_DEFINE_TYPE (StatusIconsToggle, status_icons_toggle, PHOSH_TYPE_QUICK_SETTING)

static gboolean
is_visible (void)
{
  char *path = g_build_filename (g_get_home_dir (), VISIBLE_FLAG, NULL);
  gboolean exists = g_file_test (path, G_FILE_TEST_EXISTS);
  g_free (path);
  return exists;
}

static void
update_icon (StatusIconsToggle *self)
{
  gboolean visible = is_visible ();
  phosh_status_icon_set_icon_name (self->status_icon,
    visible ? "view-reveal-symbolic" : "view-conceal-symbolic");
  phosh_status_icon_set_info (self->status_icon,
    visible ? "Иконки: видны" : "Иконки: скрыты");
}

static gboolean
on_toggle_done (gpointer data)
{
  StatusIconsToggle *self = STATUS_ICONS_TOGGLE (data);
  phosh_quick_setting_set_active (PHOSH_QUICK_SETTING (self), is_visible ());
  update_icon (self);
  return FALSE;
}

static void
do_toggle (StatusIconsToggle *self)
{
  char *script = g_build_filename (g_get_home_dir (),
    ".local", "bin", "toggle-status-icons.sh", NULL);

  if (g_file_test (script, G_FILE_TEST_EXISTS)) {
    char *argv[] = { script, NULL };
    g_spawn_async (NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);
  } else {
    gboolean active = phosh_quick_setting_get_active (PHOSH_QUICK_SETTING (self));
    char *flag = g_build_filename (g_get_home_dir (), VISIBLE_FLAG, NULL);
    if (!active)
      g_file_set_contents (flag, "", 0, NULL);
    else
      unlink (flag);
    g_free (flag);
  }
  g_free (script);

  g_timeout_add (300, on_toggle_done, self);
}

static gboolean
on_button_release (GtkWidget *widget, GdkEventButton *event, StatusIconsToggle *self)
{
  do_toggle (self);
  return FALSE;
}

static void
status_icons_toggle_dispose (GObject *object)
{
  G_OBJECT_CLASS (status_icons_toggle_parent_class)->dispose (object);
}

static void
status_icons_toggle_class_init (StatusIconsToggleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = status_icons_toggle_dispose;
}

static void
status_icons_toggle_init (StatusIconsToggle *self)
{
  self->status_icon = PHOSH_STATUS_ICON (phosh_status_icon_new ());
  phosh_status_icon_set_pixel_size (self->status_icon, 16);
  update_icon (self);
  gtk_widget_set_visible (GTK_WIDGET (self->status_icon), TRUE);

  g_object_set (self, "status-icon", self->status_icon, NULL);

  phosh_quick_setting_set_active (PHOSH_QUICK_SETTING (self), is_visible ());
  g_signal_connect (self, "button-release-event", G_CALLBACK (on_button_release), self);
}

char **
g_io_phosh_plugin_status_icons_toggle_query (void)
{
  char *ext[] = { PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET, NULL };
  return g_strdupv (ext);
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));
  g_io_extension_point_implement (PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
                                  TYPE_STATUS_ICONS_TOGGLE,
                                  PLUGIN_NAME, 10);
}

void
g_io_module_unload (GIOModule *module)
{
}
