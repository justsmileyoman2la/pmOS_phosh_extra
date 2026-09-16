# Phosh Extras — контекст проекта

## Устройство
- Xiaomi miatoll, postmarketOS edge (musl libc)
- Phosh 0.57.0, kernel 6.14.7-sm7125
- SSH: `sshpass -p "123" ssh js@192.168.1.115`
- User: `js`, password: `123`
- Пакеты: gcc 15.2.0, meson, pkg-config, openssh, git

## Репозиторий
- GitHub: https://github.com/justsmileyoman2la/pmOS_phosh_extra
- Local: `~/phosh-extras/`
- SSH ключ: `~/.ssh/id_ed25519` (уже добавлен в GitHub)

## Плагины (5 шт.)

### 1. weather-lockscreen (`src/weather-lockscreen.c`)
- Погода на экране блокировки
- Open-Meteo API (без ключа)
- Конфиг: `~/.config/phosh-weather.conf` (lat/lon)
- Обновление каждые 30 минут
- Русские/английские названия погоды

### 2. weather-quick-setting (`src/weather-quick-setting.c`)
- Температура как плитка в быстрых настройках
- Open-Meteo API

### 3. weather-status (`src/weather-status.c`)
- Температура как иконка в верхней панели
- PhoshStatusIcon, не PhoshQuickSetting
- Использует `status-icon.h` из headers/

### 4. status-icons-toggle (`src/status-icons-toggle.c`)
- Плитка в quick settings (toggle)
- **Свайп по верхней панели** (вправо = показать, влево = скрыть)
- Ищет PhoshTopPanel через `gtk_window_list_toplevels()`
- Привязывает GtkGestureSwipe через `g_idle_add`
- Скрипт: `~/.local/bin/toggle-status-icons.sh`

### 5. indicators-revealer (`src/indicators-revealer.c`)
- GtkRevealer subclass
- Pollит файл `~/.config/phosh-indicators-visible` каждые 500мс
- Файл есть → показать, нет → скрыть

## UI Overlay
- `overlay/top-panel.ui` → копируется в `~/phosh-overlay/top-panel.ui`
- `box_network` (wifi, wwan) обёрнут в `PhoshIndicatorsRevealer`
- `box_indicators` (bt, vpn, battery) обёрнут в `PhoshIndicatorsRevealer`
- Оба скрываются по флагу `~/.config/phosh-indicators-visible`

## Сборка
```bash
cd ~/phosh-extras
make                    # собрать плагины
sudo make install-plugins  # установить в /usr/lib/phosh/plugins/
make install            # установить скрипт + overlay
```

## Зависимости
- gtk+-3.0
- json-glib-1.0
- libsoup-3.0 (НЕ 2.4!)
- phosh (заголовки)

## Важные находки
1. `PhoshQuickSetting` — это GtkBox, НЕ GtkToggleButton
2. `notify::active` не срабатывает для плагинов — используем `button-release-event`
3. `PhoshTopPanel` — это toplevel window (не child другого toplevel)
4. `gtk_container_foreach` callback должен быть `void`, не `gboolean`
5. `gtk_gesture_swipe_new(widget)` — GTK3 API (не GTK4)
6. GTK CSS плагинов не переопределяет Phosh CSS — opacity не работает
7. Widget tree traversal не работает для PhoshTopPanel (layer surface)

## Текущий gsettings
```bash
gsettings get sm.puri.phosh.plugins quick-settings
# ["status-icons-toggle", "weather-quick-setting", "mobile-data-quick-setting", "indicators-revealer"]
```

## Git коммиты
1. `2fee960` — Initial release
2. `9b6569c` — Add weather-status plugin
3. `0ff63c6` — Add panel swipe

## Что можно дальше
- Добавить виджет погоды в overlay (не только на lockscreen)
- Настроить lat/lon через quick settings
- Добавить анимацию скрытия/показа
- Опубликовать на postmarketOS wiki/forum
