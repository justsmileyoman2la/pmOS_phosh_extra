#!/bin/sh
[ -z "$1" ] && echo "Usage: $0 <input.gif>" && exit 1
mkdir -p ~/wallpaper/frames
rm -f ~/wallpaper/frames/*.png
ffmpeg -i "$1" -vf "scale=1080:2400:flags=lanczos:force_original_aspect_ratio=decrease,pad=1080:2400:(ow-iw)/2:(oh-ih)/2:black" ~/wallpaper/frames/frame_%04d.png
echo "Done: $(ls ~/wallpaper/frames/*.png 2>/dev/null | wc -l) frames"
