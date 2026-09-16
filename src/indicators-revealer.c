/*
 * PhoshIndicatorsRevealer - GtkRevealer subclass for status bar icons
 *
 * Watches ~/.config/phosh-indicators-visible file via polling:
 *   file exists → icons visible
 *   file absent → icons hidden
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <phosh-plugin.h>

#define PLUGIN_NAME "indicators-revealer"
#define VISIBLE_FLAG ".config/phosh-indicators-visible"
#define POLL_MS 500

typedef struct _PhoshIndicatorsRevealer {
  GtkRevealer   parent;
  gboolean      last_visible;
} PhoshIndicatorsRevealer;

typedef struct _PhoshIndicatorsRevealerClass {
  GtkRevealerClass parent_class;
} PhoshIndicatorsRevealerClass;

#define PHOSH_TYPE_INDICATORS_REVEALER phosh_indicators_revealer_get_type ()
#define PHOSH_INDICATORS_REVEALER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_INDICATORS_REVEALER, PhoshIndicatorsRevealer))
G_DEFINE_TYPE (PhoshIndicatorsRevealer, phosh_indicators_revealer, GTK_TYPE_REVEALER)

static gboolean
check_visible (void)
{
  char *path = g_build_filename (g_get_home_dir (), VISIBLE_FLAG, NULL);
  gboolean exists = g_file_test (path, G_FILE_TEST_EXISTS);
  g_free (path);
  return exists;
}

static gboolean
poll_visible (gpointer data)
{
  PhoshIndicatorsRevealer *self = PHOSH_INDICATORS_REVEALER (data);
  gboolean visible = check_visible ();
  if (visible != self->last_visible) {
    self->last_visible = visible;
    gtk_revealer_set_reveal_child (GTK_REVEALER (self), visible);
  }
  return G_SOURCE_CONTINUE;
}

static void
phosh_indicators_revealer_dispose (GObject *object)
{
  G_OBJECT_CLASS (phosh_indicators_revealer_parent_class)->dispose (object);
}

static void
phosh_indicators_revealer_class_init (PhoshIndicatorsRevealerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = phosh_indicators_revealer_dispose;
}

static void
phosh_indicators_revealer_init (PhoshIndicatorsRevealer *self)
{
  gtk_revealer_set_transition_type (GTK_REVEALER (self),
                                    GTK_REVEALER_TRANSITION_TYPE_NONE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (self), 0);

  self->last_visible = check_visible ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), self->last_visible);

  g_timeout_add (POLL_MS, poll_visible, self);
}

/* ---- plugin entry points ---- */

char **
g_io_phosh_plugin_indicators_revealer_query (void)
{
  char *ext[] = {
    PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
    NULL
  };
  return g_strdupv (ext);
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));
  g_io_extension_point_implement (PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
                                  PHOSH_TYPE_INDICATORS_REVEALER,
                                  PLUGIN_NAME, 10);
}

void
g_io_module_unload (GIOModule *module)
{
}
