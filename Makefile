PLUGIN_DIR ?= /usr/lib/phosh/plugins
HEADERS    = headers

GTK_CFLAGS  = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS    = $(shell pkg-config --libs gtk+-3.0)
JSON_CFLAGS = $(shell pkg-config --cflags json-glib-1.0)
JSON_LIBS   = $(shell pkg-config --libs json-glib-1.0)
SOUP_CFLAGS = $(shell pkg-config --cflags libsoup-3.0)
SOUP_LIBS   = $(shell pkg-config --libs libsoup-3.0)
PHOSH_INC   = -I/usr/include/phosh -I$(HEADERS)

ALL_CFLAGS  = $(GTK_CFLAGS) $(JSON_CFLAGS) $(SOUP_CFLAGS) $(PHOSH_INC)
ALL_LIBS    = $(GTK_LIBS) $(JSON_LIBS) $(SOUP_LIBS)

PLUGINS = \
  libphosh-plugin-weather.so \
  libphosh-plugin-weather-quick-setting.so \
  libphosh-plugin-status-icons-toggle.so \
  libphosh-plugin-indicators-revealer.so

all: $(PLUGINS)

libphosh-plugin-weather.so: src/weather-lockscreen.c
	gcc -shared -fPIC -o $@ $< $(ALL_CFLAGS) $(ALL_LIBS)

libphosh-plugin-weather-quick-setting.so: src/weather-quick-setting.c
	gcc -shared -fPIC -o $@ $< $(ALL_CFLAGS) $(ALL_LIBS)

libphosh-plugin-status-icons-toggle.so: src/status-icons-toggle.c
	gcc -shared -fPIC -o $@ $< $(GTK_CFLAGS) $(GTK_LIBS) $(PHOSH_INC)

libphosh-plugin-indicators-revealer.so: src/indicators-revealer.c
	gcc -shared -fPIC -o $@ $< $(GTK_CFLAGS) $(GTK_LIBS) $(PHOSH_INC)

install: all
	mkdir -p ~/.local/bin
	cp scripts/toggle-status-icons.sh ~/.local/bin/
	mkdir -p ~/phosh-overlay
	cp overlay/top-panel.ui ~/phosh-overlay/
	@echo ""
	@echo "Plugins built. To install system-wide:"
	@echo "  sudo make install-plugins"

install-plugins: all
	echo "123" | sudo -S mkdir -p $(PLUGIN_DIR)
	echo "123" | sudo -S cp *.so $(PLUGIN_DIR)/
	echo "123" | sudo -S cp plugins/*.plugin $(PLUGIN_DIR)/

clean:
	rm -f *.so

.PHONY: all install install-plugins clean
