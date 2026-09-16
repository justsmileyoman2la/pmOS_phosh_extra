#!/bin/bash
FLAG=~/.config/phosh-indicators-visible
if [ -f "$FLAG" ]; then
  rm "$FLAG"
  echo "Status icons: HIDDEN"
else
  touch "$FLAG"
  echo "Status icons: VISIBLE"
fi
