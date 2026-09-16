/*
 * phosh weather status icon plugin
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shows current temperature (from Open-Meteo) as a status icon
 * in the top panel bar.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "status-icon.h"
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <phosh-plugin.h>

#define PLUGIN_NAME  "weather-status"

#define WEATHER_URL  "https://api.open-meteo.com/v1/forecast"
#define DEFAULT_LAT  "55.7558"
#define DEFAULT_LON  "37.6173"
#define REFRESH_SECS (30 * 60)

typedef struct _PhoshWeatherStatusIcon {
  PhoshStatusIcon parent;

  SoupSession    *session;
  guint           refresh_timeout;
  char           *latitude;
  char           *longitude;
} PhoshWeatherStatusIcon;

typedef struct _PhoshWeatherStatusIconClass {
  PhoshStatusIconClass parent_class;
} PhoshWeatherStatusIconClass;

#define PHOSH_TYPE_WEATHER_STATUS_ICON phosh_weather_status_icon_get_type ()
#define PHOSH_WEATHER_STATUS_ICON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_WEATHER_STATUS_ICON, PhoshWeatherStatusIcon))
G_DEFINE_TYPE (PhoshWeatherStatusIcon, phosh_weather_status_icon, PHOSH_TYPE_STATUS_ICON)

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
  case 0:  return ru ? "Ясно"        : "Clear";
  case 1:  return ru ? "Почти ясно"  : "Mostly clear";
  case 2:  return ru ? "Облачно"     : "Cloudy";
  case 3:  return ru ? "Пасмурно"    : "Overcast";
  case 45: case 48:
           return ru ? "Туман"       : "Fog";
  case 51: case 53: case 55:
           return ru ? "Морось"      : "Drizzle";
  case 56: case 57:
           return ru ? "Лед. морось" : "Freezing drizzle";
  case 61: case 63:
           return ru ? "Дождь"       : "Rain";
  case 65: return ru ? "Сильный дождь" : "Heavy rain";
  case 66: case 67:
           return ru ? "Лед. дождь"  : "Freezing rain";
  case 71: case 73:
           return ru ? "Снег"        : "Snow";
  case 75: return ru ? "Сильный снег" : "Heavy snow";
  case 77: return ru ? "Снежные зёрна" : "Snow grains";
  case 80: case 81: case 82:
           return ru ? "Ливень"      : "Rain showers";
  case 85: case 86:
           return ru ? "Снегопад"    : "Snow showers";
  case 95: return ru ? "Гроза"       : "Thunderstorm";
  case 96: case 99:
           return ru ? "Гроза с градом" : "Thunderstorm, hail";
  default: return ru ? "—"           : "—";
  }
}

static const char *
weather_icon (gint64 code)
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

static gboolean
parse_and_update (PhoshWeatherStatusIcon *self, GBytes *bytes)
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

    phosh_status_icon_set_icon_name (PHOSH_STATUS_ICON (self), weather_icon (code));
    tmp = g_strdup_printf ("%.0f° %s", temp, weather_condition (code, weather_is_ru ()));
    phosh_status_icon_set_info (PHOSH_STATUS_ICON (self), tmp);
    g_free (tmp);
    ok = TRUE;
  }

out:
  g_object_unref (parser);
  return ok;
}

static void
on_response (GObject *session, GAsyncResult *res, gpointer user_data)
{
  PhoshWeatherStatusIcon *self = user_data;
  GBytes *bytes;
  GError *error = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (session), res, &error);
  if (error) {
    g_warning ("phosh-weather-status: fetch failed: %s", error->message);
    g_clear_error (&error);
    return;
  }

  parse_and_update (self, bytes);
  g_bytes_unref (bytes);
}

static void
phosh_weather_status_icon_fetch (PhoshWeatherStatusIcon *self)
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
  PhoshWeatherStatusIcon *self = user_data;
  phosh_weather_status_icon_fetch (self);
  return G_SOURCE_CONTINUE;
}

static void
phosh_weather_status_icon_dispose (GObject *object)
{
  PhoshWeatherStatusIcon *self = PHOSH_WEATHER_STATUS_ICON (object);

  g_clear_handle_id (&self->refresh_timeout, g_source_remove);

  if (self->session)
    soup_session_abort (self->session);

  g_clear_object (&self->session);
  g_clear_pointer (&self->latitude, g_free);
  g_clear_pointer (&self->longitude, g_free);

  G_OBJECT_CLASS (phosh_weather_status_icon_parent_class)->dispose (object);
}

static void
phosh_weather_status_icon_class_init (PhoshWeatherStatusIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = phosh_weather_status_icon_dispose;
}

static void
phosh_weather_status_icon_init (PhoshWeatherStatusIcon *self)
{
  self->latitude = g_strdup (DEFAULT_LAT);
  self->longitude = g_strdup (DEFAULT_LON);
  self->session = soup_session_new ();

  phosh_status_icon_set_pixel_size (PHOSH_STATUS_ICON (self), 16);
  phosh_status_icon_set_icon_name (PHOSH_STATUS_ICON (self),
                                   "weather-none-available-symbolic");
  phosh_status_icon_set_info (PHOSH_STATUS_ICON (self), "--°");

  phosh_weather_status_icon_fetch (self);
  self->refresh_timeout = g_timeout_add_seconds (REFRESH_SECS,
                                                 on_refresh_timeout, self);
}

/* ---- plugin entry points ---- */

char **
g_io_phosh_plugin_weather_status_icon_query (void)
{
  char *extension_points[] = {
    PHOSH_PLUGIN_EXTENSION_POINT_STATUS_ICON_WIDGET,
    NULL
  };
  return g_strdupv (extension_points);
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));
  g_io_extension_point_implement (PHOSH_PLUGIN_EXTENSION_POINT_STATUS_ICON_WIDGET,
                                  PHOSH_TYPE_WEATHER_STATUS_ICON,
                                  PLUGIN_NAME, 10);
}

void
g_io_module_unload (GIOModule *module)
{
}
