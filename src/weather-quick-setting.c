/*
 * phosh weather quick-setting plugin
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shows current temperature (from Open-Meteo) as a quick-setting tile
 * in the expanded notification/quick-settings panel.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <phosh-plugin.h>

#include "quick-setting.h"

#define PLUGIN_NAME  "weather-quick-setting"

#define WEATHER_URL  "https://api.open-meteo.com/v1/forecast"
#define DEFAULT_LAT  "55.7558"
#define DEFAULT_LON  "37.6173"
#define REFRESH_SECS (30 * 60)

typedef struct _PhoshWeatherQuickSetting {
  PhoshQuickSetting parent;

  SoupSession     *session;
  PhoshStatusIcon *status_icon;
  guint            refresh_timeout;
  char            *latitude;
  char            *longitude;
} PhoshWeatherQuickSetting;

typedef struct _PhoshWeatherQuickSettingClass {
  PhoshQuickSettingClass parent_class;
} PhoshWeatherQuickSettingClass;

#define PHOSH_TYPE_WEATHER_QUICK_SETTING phosh_weather_quick_setting_get_type ()
#define PHOSH_WEATHER_QUICK_SETTING(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_WEATHER_QUICK_SETTING, PhoshWeatherQuickSetting))
G_DEFINE_TYPE (PhoshWeatherQuickSetting, phosh_weather_quick_setting, PHOSH_TYPE_QUICK_SETTING)

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

static gboolean
parse_and_update (PhoshWeatherQuickSetting *self, GBytes *bytes)
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
    const char *icon;
    char *tmp;

    switch (code) {
    case 0:  icon = "weather-clear-symbolic";  break;
    case 1:  icon = "weather-few-clouds-symbolic"; break;
    case 2:  icon = "weather-few-clouds-symbolic"; break;
    case 3:  icon = "weather-overcast-symbolic"; break;
    case 45: case 48: icon = "weather-fog-symbolic"; break;
    case 51: case 53: case 55: case 56: case 57:
             icon = "weather-showers-scattered-symbolic"; break;
    case 61: case 63: case 65: case 66: case 67:
             icon = "weather-showers-symbolic"; break;
    case 71: case 73: case 75: case 77:
             icon = "weather-snow-symbolic"; break;
    case 80: case 81: case 82:
             icon = "weather-showers-symbolic"; break;
    case 85: case 86:
             icon = "weather-snow-symbolic"; break;
    case 95: case 96: case 99:
             icon = "weather-storm-symbolic"; break;
    default: icon = "weather-none-available-symbolic"; break;
    }

    phosh_status_icon_set_icon_name (self->status_icon, icon);
    tmp = g_strdup_printf ("%.0f° %s", temp, weather_condition (code, weather_is_ru ()));
    phosh_status_icon_set_info (self->status_icon, tmp);
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
  PhoshWeatherQuickSetting *self = user_data;
  GBytes *bytes;
  GError *error = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (session), res,
                                             &error);
  if (error) {
    g_warning ("phosh-weather-quick-setting: fetch failed: %s",
               error->message);
    g_clear_error (&error);
    return;
  }

  parse_and_update (self, bytes);
  g_bytes_unref (bytes);
}

static void
phosh_weather_quick_setting_fetch (PhoshWeatherQuickSetting *self)
{
  SoupMessage *msg;
  char *url;

  url = g_strdup_printf ("%s?latitude=%s&longitude=%s&current_weather=true",
                         WEATHER_URL, self->latitude, self->longitude);
  msg = soup_message_new ("GET", url);
  g_free (url);

  soup_session_send_and_read_async (self->session, msg, 0, NULL,
                                    on_response, self);
  g_object_unref (msg);
}

static gboolean
on_refresh_timeout (gpointer user_data)
{
  PhoshWeatherQuickSetting *self = user_data;

  phosh_weather_quick_setting_fetch (self);
  return G_SOURCE_CONTINUE;
}

static void
phosh_weather_quick_setting_dispose (GObject *object)
{
  PhoshWeatherQuickSetting *self = PHOSH_WEATHER_QUICK_SETTING (object);

  g_clear_handle_id (&self->refresh_timeout, g_source_remove);

  if (self->session)
    soup_session_abort (self->session);

  g_clear_object (&self->session);
  g_clear_pointer (&self->latitude, g_free);
  g_clear_pointer (&self->longitude, g_free);

  G_OBJECT_CLASS (phosh_weather_quick_setting_parent_class)->dispose (object);
}

static void
phosh_weather_quick_setting_class_init (PhoshWeatherQuickSettingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = phosh_weather_quick_setting_dispose;
}

static void
phosh_weather_quick_setting_init (PhoshWeatherQuickSetting *self)
{
  self->latitude = g_strdup (DEFAULT_LAT);
  self->longitude = g_strdup (DEFAULT_LON);
  self->session = soup_session_new ();

  self->status_icon = PHOSH_STATUS_ICON (phosh_status_icon_new ());
  phosh_status_icon_set_pixel_size (self->status_icon, 16);
  phosh_status_icon_set_icon_name (self->status_icon,
                                   "weather-none-available-symbolic");
  phosh_status_icon_set_info (self->status_icon, "--°");
  gtk_widget_set_visible (GTK_WIDGET (self->status_icon), TRUE);

  g_object_set (self, "status-icon", self->status_icon, NULL);

  phosh_weather_quick_setting_fetch (self);
  self->refresh_timeout = g_timeout_add_seconds (REFRESH_SECS,
                                                 on_refresh_timeout, self);
}

/* ---- plugin entry points ---- */

char **
g_io_phosh_plugin_weather_quick_setting_query (void)
{
  char *extension_points[] = {
    PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
    NULL
  };

  return g_strdupv (extension_points);
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

  g_io_extension_point_implement (PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
                                  PHOSH_TYPE_WEATHER_QUICK_SETTING,
                                  PLUGIN_NAME,
                                  10);
}

void
g_io_module_unload (GIOModule *module)
{
}
