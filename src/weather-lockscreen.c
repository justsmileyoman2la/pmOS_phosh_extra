/*
 * phosh weather lockscreen plugin
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shows current weather (temperature + conditions) on the lock screen.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <phosh-plugin.h>

#define PLUGIN_NAME  "weather"

#define WEATHER_URL  "https://api.open-meteo.com/v1/forecast"
#define DEFAULT_LAT  "55.7558"
#define DEFAULT_LON  "37.6173"
#define REFRESH_SECS (30 * 60)
#define CONFIG_PATH  ".config/phosh-weather.conf"

typedef struct _PhoshWeatherWidget {
  GtkBox          parent;

  SoupSession    *session;
  GtkWidget      *icon_image;
  GtkWidget      *temp_label;
  GtkWidget      *cond_label;
  GtkWidget      *time_label;
  GDateTime      *fetch_time;
  guint           refresh_timeout;
  guint           time_update_id;
  char           *latitude;
  char           *longitude;
} PhoshWeatherWidget;

typedef struct _PhoshWeatherWidgetClass {
  GtkBoxClass parent_class;
} PhoshWeatherWidgetClass;

#define PHOSH_TYPE_WEATHER_WIDGET phosh_weather_widget_get_type ()
#define PHOSH_WEATHER_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_WEATHER_WIDGET, PhoshWeatherWidget))
G_DEFINE_TYPE (PhoshWeatherWidget, phosh_weather_widget, GTK_TYPE_BOX)

static void
load_config (PhoshWeatherWidget *self)
{
  char *path = g_build_filename (g_get_home_dir (), CONFIG_PATH, NULL);
  gchar *contents = NULL;
  gsize len = 0;

  self->latitude = g_strdup (DEFAULT_LAT);
  self->longitude = g_strdup (DEFAULT_LON);

  if (g_file_get_contents (path, &contents, &len, NULL)) {
    gchar **lines = g_strsplit (contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
      if (g_str_has_prefix (lines[i], "lat="))
        g_free (g_steal_pointer (&self->latitude)),
        self->latitude = g_strdup (lines[i] + 4);
      else if (g_str_has_prefix (lines[i], "lon="))
        g_free (g_steal_pointer (&self->longitude)),
        self->longitude = g_strdup (lines[i] + 4);
    }
    g_strfreev (lines);
    g_free (contents);
  }
  g_free (path);
}

static gboolean
weather_is_ru (void)
{
  const char * const *langs = g_get_language_names ();
  for (int i = 0; langs[i]; i++) {
    if (g_str_has_prefix (langs[i], "ru"))
      return TRUE;
  }
  return FALSE;
}

static const char *
weather_condition (gint64 code, gboolean ru)
{
  switch (code) {
  case 0:  return ru ? "Ясно"            : "Clear";
  case 1:  return ru ? "Почти ясно"      : "Mostly clear";
  case 2:  return ru ? "Облачно"         : "Partly cloudy";
  case 3:  return ru ? "Пасмурно"        : "Overcast";
  case 45: case 48:
           return ru ? "Туман"           : "Fog";
  case 51: case 53: case 55:
           return ru ? "Морось"          : "Drizzle";
  case 56: case 57:
           return ru ? "Лед. морось"     : "Freezing drizzle";
  case 61: case 63:
           return ru ? "Дождь"           : "Rain";
  case 65: return ru ? "Сильный дождь"   : "Heavy rain";
  case 66: case 67:
           return ru ? "Лед. дождь"      : "Freezing rain";
  case 71: case 73:
           return ru ? "Снег"            : "Snow";
  case 75: return ru ? "Сильный снег"    : "Heavy snow";
  case 77: return ru ? "Снежные зёрна"   : "Snow grains";
  case 80: case 81: case 82:
           return ru ? "Ливень"          : "Rain showers";
  case 85: case 86:
           return ru ? "Снегопад"        : "Snow showers";
  case 95: return ru ? "Гроза"           : "Thunderstorm";
  case 96: case 99:
           return ru ? "Гроза с градом"  : "Thunderstorm with hail";
  default: return "—";
  }
}

static const char *
weather_icon_name (gint64 code)
{
  switch (code) {
  case 0:  return "weather-clear-symbolic";
  case 1:  return "weather-few-clouds-symbolic";
  case 2:  return "weather-few-clouds-symbolic";
  case 3:  return "weather-overcast-symbolic";
  case 45: case 48: return "weather-fog-symbolic";
  case 51: case 53: case 55: case 56: case 57:
           return "weather-showers-scattered-symbolic";
  case 61: case 63: case 65: case 66: case 67:
           return "weather-showers-symbolic";
  case 71: case 73: case 75: case 77:
           return "weather-snow-symbolic";
  case 80: case 81: case 82:
           return "weather-showers-symbolic";
  case 85: case 86:
           return "weather-snow-symbolic";
  case 95: case 96: case 99:
           return "weather-storm-symbolic";
  default: return "weather-none-available-symbolic";
  }
}

static void
update_time_label (PhoshWeatherWidget *self)
{
  char *markup;

  if (self->fetch_time == NULL) {
    gtk_label_set_markup (GTK_LABEL (self->time_label), "");
    return;
  }

  GDateTime *now = g_date_time_new_now_local ();
  GTimeSpan diff = g_date_time_difference (now, self->fetch_time);
  gint64 mins = diff / G_USEC_PER_SEC / 60;
  g_date_time_unref (now);

  if (mins < 1)
    markup = g_strdup (weather_is_ru () ? "<small>обновлено только что</small>"
                                        : "<small>just updated</small>");
  else if (weather_is_ru ())
    markup = g_strdup_printf ("<small>обновлено %lld мин. назад</small>", (long long)mins);
  else
    markup = g_strdup_printf ("<small>updated %lld min ago</small>", (long long)mins);

  gtk_label_set_markup (GTK_LABEL (self->time_label), markup);
  g_free (markup);
}

static gboolean
on_time_update (gpointer user_data)
{
  update_time_label (PHOSH_WEATHER_WIDGET (user_data));
  return G_SOURCE_CONTINUE;
}

static gboolean
parse_and_update (PhoshWeatherWidget *self, GBytes *bytes)
{
  JsonParser *parser;
  JsonObject *root_obj, *cw;
  const guchar *data;
  gsize size;
  gboolean ok = FALSE;

  if (bytes == NULL)
    return FALSE;

  data = g_bytes_get_data (bytes, &size);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, (const char *) data, size, NULL))
    goto out;

  if (!JSON_NODE_HOLDS_OBJECT (json_parser_get_root (parser)))
    goto out;

  root_obj = json_node_get_object (json_parser_get_root (parser));
  cw = json_object_get_object_member (root_obj, "current_weather");
  if (!cw)
    goto out;

  {
    double temp = json_object_get_double_member (cw, "temperature");
    gint64 code = json_object_get_int_member (cw, "weathercode");
    char *tmp;

    gtk_image_set_from_icon_name (GTK_IMAGE (self->icon_image),
                                  weather_icon_name (code), GTK_ICON_SIZE_DIALOG);

    tmp = g_strdup_printf ("%.0f°", temp);
    gtk_label_set_markup (GTK_LABEL (self->temp_label), tmp);
    g_free (tmp);

    tmp = g_strdup_printf ("%s", weather_condition (code, weather_is_ru ()));
    gtk_label_set_markup (GTK_LABEL (self->cond_label), tmp);
    g_free (tmp);

    {
      g_clear_pointer (&self->fetch_time, g_date_time_unref);
      self->fetch_time = g_date_time_new_now_local ();
      update_time_label (self);
    }

    ok = TRUE;
  }

out:
  g_object_unref (parser);
  return ok;
}

static void
on_response (GObject *session, GAsyncResult *res, gpointer user_data)
{
  PhoshWeatherWidget *self = user_data;
  GBytes *bytes;
  GError *error = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (session), res, &error);
  if (error) {
    g_warning ("phosh-weather: fetch failed: %s", error->message);
    g_clear_error (&error);
    return;
  }

  parse_and_update (self, bytes);
  g_bytes_unref (bytes);
}

static void
phosh_weather_widget_fetch (PhoshWeatherWidget *self)
{
  SoupMessage *msg;
  char *url;

  url = g_strdup_printf ("%s?latitude=%s&longitude=%s&current_weather=true",
                         WEATHER_URL, self->latitude, self->longitude);
  msg = soup_message_new ("GET", url);
  g_free (url);

  soup_session_send_and_read_async (self->session, msg, 0, NULL, on_response, self);
  g_object_unref (msg);
}

static gboolean
on_refresh_timeout (gpointer user_data)
{
  PhoshWeatherWidget *self = user_data;
  phosh_weather_widget_fetch (self);
  return G_SOURCE_CONTINUE;
}

static void
phosh_weather_widget_dispose (GObject *object)
{
  PhoshWeatherWidget *self = PHOSH_WEATHER_WIDGET (object);

  g_clear_handle_id (&self->refresh_timeout, g_source_remove);
  g_clear_handle_id (&self->time_update_id, g_source_remove);

  if (self->session)
    soup_session_abort (self->session);
  g_clear_object (&self->session);
  g_clear_pointer (&self->fetch_time, g_date_time_unref);
  g_clear_pointer (&self->latitude, g_free);
  g_clear_pointer (&self->longitude, g_free);

  G_OBJECT_CLASS (phosh_weather_widget_parent_class)->dispose (object);
}

static void
phosh_weather_widget_class_init (PhoshWeatherWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = phosh_weather_widget_dispose;
}

static void
phosh_weather_widget_init (PhoshWeatherWidget *self)
{
  load_config (self);
  self->session = soup_session_new ();

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_box_set_spacing (GTK_BOX (self), 8);
  gtk_widget_set_name (GTK_WIDGET (self), "phosh-lockscreen-widget");

  self->icon_image = gtk_image_new ();
  gtk_image_set_from_icon_name (GTK_IMAGE (self->icon_image),
                                "weather-none-available-symbolic",
                                GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (self->icon_image);
  gtk_container_add (GTK_CONTAINER (self), self->icon_image);

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (self), vbox);

  self->temp_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (self->temp_label), "--°");
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (4.0));
  gtk_label_set_attributes (GTK_LABEL (self->temp_label), attrs);
  pango_attr_list_unref (attrs);
  gtk_widget_show (self->temp_label);
  gtk_container_add (GTK_CONTAINER (vbox), self->temp_label);

  self->cond_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (self->cond_label), "—");
  gtk_label_set_line_wrap (GTK_LABEL (self->cond_label), TRUE);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (2.0));
  gtk_label_set_attributes (GTK_LABEL (self->cond_label), attrs);
  pango_attr_list_unref (attrs);
  gtk_widget_show (self->cond_label);
  gtk_container_add (GTK_CONTAINER (vbox), self->cond_label);

  self->time_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (self->time_label), "");
  gtk_widget_set_name (self->time_label, "weather-time-label");
  gtk_widget_set_margin_top (GTK_WIDGET (self->time_label), 8);
  gtk_widget_show (self->time_label);
  gtk_container_add (GTK_CONTAINER (vbox), self->time_label);

  phosh_weather_widget_fetch (self);
  self->refresh_timeout = g_timeout_add_seconds (REFRESH_SECS, on_refresh_timeout, self);
  self->time_update_id = g_timeout_add_seconds (60, on_time_update, self);
}

/* ---- plugin entry points ---- */

char **
g_io_phosh_plugin_weather_query (void)
{
  char *extension_points[] = {
    PHOSH_PLUGIN_EXTENSION_POINT_LOCKSCREEN_WIDGET,
    NULL
  };
  return g_strdupv (extension_points);
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

  g_io_extension_point_implement (PHOSH_PLUGIN_EXTENSION_POINT_LOCKSCREEN_WIDGET,
                                  PHOSH_TYPE_WEATHER_WIDGET,
                                  PLUGIN_NAME,
                                  10);
}

void
g_io_module_unload (GIOModule *module)
{
}
