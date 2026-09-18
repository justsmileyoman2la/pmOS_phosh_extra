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

### Animated GIF Wallpaper
Cycles through PNG frames as a desktop background using `gsettings`.
Converts any GIF to 1080x2400 frames with `ffmpeg`.

- Place frames in `~/wallpaper/frames/frame_NNNN.png`
- Runs as systemd user service with auto-restart
- Also available as XDG autostart entry

## Installation

### Prerequisites

```bash
# postmarketOS
sudo apk add gtk+3.0-dev phosh-dev json-glib-dev libsoup-dev meson gcc ffmpeg

# Debian/Ubuntu
sudo apt install libgtk-3-dev phosh-dev libjson-glib-dev libsoup2.4-dev meson gcc ffmpeg
Build & Install Plugins
make                    # build plugins
sudo make install-plugins  # install to /usr/lib/phosh/plugins/
make install            # install script + overlay to home directory
Animated Wallpaper
cd animated-wallpaper
./install.sh    # installs scripts + systemd service
Convert a GIF:
./convert-gif.sh your-animation.gif
systemctl --user start gif-wallpaper.service
Enable Plugins
gsettings set sm.puri.phosh.plugins quick-settings \
  "[status-icons-toggle, weather-quick-setting, mobile-data-quick-setting]"

gsettings set sm.puri.phosh.plugins lockscreen \
  "[weather]"
UI Overlay (for Status Icons Toggle)
The Status Icons Toggle requires a modified top-panel.ui overlay.
This is automatically installed to ~/phosh-overlay/top-panel.ui by make install.
Restart Phosh
loginctl terminate-session $(loginctl list-sessions --no-legend | grep seat0 | awk '{print}')
Configuration
Weather
Create ~/.config/phosh-weather.conf:
lat=55.7558
lon=37.6173
Status Icons Toggle
The toggle creates/deletes ~/.config/phosh-indicators-visible:
- File exists → icons visible
- File absent → icons hidden
~/.local/bin/toggle-status-icons.sh
Animated Wallpaper
Place frames in ~/wallpaper/frames/ as frame_0001.png, frame_0002.png, etc.
Use convert-gif.sh to extract frames from any GIF:
./convert-gif.sh ~/Downloads/animation.gif
systemctl --user start gif-wallpaper.service
Files
phosh-extras/
├── src/
│   ├── weather-lockscreen.c
│   ├── weather-quick-setting.c
│   ├── status-icons-toggle.c
│   └── indicators-revealer.c
├── headers/
│   ├── quick-setting.h
│   ├── status-icon.h
│   └── status-page.h
├── plugins/
│   ├── weather.plugin
│   ├── weather-quick-setting.plugin
│   ├── status-icons-toggle.plugin
│   └── indicators-revealer.plugin
├── scripts/
│   └── toggle-status-icons.sh
├── overlay/
│   └── top-panel.ui
├── animated-wallpaper/
│   ├── daemon.sh
│   ├── convert-gif.sh
│   ├── gif-wallpaper.service
│   └── gif-wallpaper.desktop
├── Makefile
└── README.md
Compatibility
- Tested on postmarketOS edge with Phosh 0.57
- Should work with any Phosh 0.45+ installation
- Requires gtk+-3.0, json-glib, libsoup-2.4, ffmpeg
License
GPL-3.0-or-later
