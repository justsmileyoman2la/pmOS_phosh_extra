# Phosh Extras

Custom plugins for [Phosh](https://phosh.mobi/) — the GNOME-based mobile shell.

## Plugins

### Weather Lockscreen
Shows current weather (temperature + conditions + icon) on the lock screen.
Uses [Open-Meteo](https://open-meteo.com/) API (no API key needed).

- `lat` / `lon` — configure in `~/.config/phosh-weather.conf`
- Refreshes every 30 minutes
- Supports Russian and English condition names

### Weather Quick Setting
Shows current temperature as a quick-setting tile in the pull-down panel.

### Status Icons Toggle
Quick-setting tile to hide/show all status bar icons (wifi, bluetooth, battery, etc.).
When hidden, only the clock remains visible in the top panel.

### Indicators Revealer
Internal widget (GtkRevealer subclass) used by Status Icons Toggle.
Polls `~/.config/phosh-indicators-visible` file to control visibility.

## Installation

### Prerequisites

```bash
# postmarketOS
sudo apk add gtk+3.0-dev phosh-dev json-glib-dev libsoup-dev meson gcc

# Debian/Ubuntu
sudo apt install libgtk-3-dev phosh-dev libjson-glib-dev libsoup2.4-dev meson gcc
```

### Build & Install

```bash
make                    # build plugins
sudo make install-plugins  # install to /usr/lib/phosh/plugins/
make install            # install script + overlay to home directory
```

### Enable Plugins

```bash
# Add plugins to Phosh config
gsettings set sm.puri.phosh.plugins quick-settings \
  "[status-icons-toggle, weather-quick-setting, mobile-data-quick-setting]"

gsettings set sm.puri.phosh.plugins lockscreen \
  "[weather]"
```

### UI Overlay (for Status Icons Toggle)

The Status Icons Toggle requires a modified `top-panel.ui` overlay.
This is automatically installed to `~/phosh-overlay/top-panel.ui` by `make install`.

The overlay wraps `box_network` and `box_indicators` in `PhoshIndicatorsRevealer`
widgets, allowing them to be hidden/shown by the toggle.

### Restart Phosh

```bash
loginctl terminate-session $(loginctl list-sessions --no-legend | grep seat0 | awk {print })
```

## Configuration

### Weather

Create `~/.config/phosh-weather.conf`:

```
lat=55.7558
lon=37.6173
```

### Status Icons Toggle

The toggle creates/deletes `~/.config/phosh-indicators-visible`:
- File exists → icons visible
- File absent → icons hidden

You can also toggle from terminal:

```bash
~/.local/bin/toggle-status-icons.sh
```

## Files

```
phosh-extras/
├── src/
│   ├── weather-lockscreen.c      # Lock screen weather widget
│   ├── weather-quick-setting.c   # Quick setting weather tile
│   ├── status-icons-toggle.c     # Quick setting toggle tile
│   └── indicators-revealer.c     # GtkRevealer for icon visibility
├── headers/
│   ├── quick-setting.h           # PhoshQuickSetting API
│   ├── status-icon.h             # PhoshStatusIcon API
│   └── status-page.h             # PhoshStatusPage API
├── plugins/
│   ├── weather.plugin
│   ├── weather-quick-setting.plugin
│   ├── status-icons-toggle.plugin
│   └── indicators-revealer.plugin
├── scripts/
│   └── toggle-status-icons.sh    # Toggle script
├── overlay/
│   └── top-panel.ui              # Modified Phosh top panel
├── Makefile
└── README.md
```

## Compatibility

- Tested on postmarketOS edge with Phosh 0.57
- Should work with any Phosh 0.45+ installation
- Requires `gtk+-3.0`, `json-glib`, `libsoup-2.4`

## License

GPL-3.0-or-later
