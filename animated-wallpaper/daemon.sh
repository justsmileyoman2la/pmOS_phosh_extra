#!/bin/sh
FRAMES="$HOME/wallpaper/frames"
COUNT=$(ls "$FRAMES"/frame_*.png 2>/dev/null | wc -l)
[ "$COUNT" -eq 0 ] && exit 1
while true; do
    i=1
    while [ "$i" -le "$COUNT" ]; do
        F=$(printf "%s/frame_%04d.png" "$FRAMES" "$i")
        [ -f "$F" ] && gsettings set org.gnome.desktop.background picture-uri "file://$F"
        [ -f "$F" ] && gsettings set org.gnome.desktop.background picture-uri-dark "file://$F"
        i=$((i + 1))
        sleep 0.08
    done
done
