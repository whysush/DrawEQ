#!/usr/bin/env bash
# Fetches the two bundled typefaces. Both are SIL Open Font License 1.1, which
# permits embedding in a commercial binary. Run once after cloning.
set -euo pipefail
cd "$(dirname "$0")/../Resources/Fonts"

fetch () {  # fetch <url> <dest>
    echo "  $2"
    curl -fsSL --retry 3 -o "$2" "$1"
}

fetch https://github.com/floriankarsten/space-grotesk/raw/master/fonts/ttf/static/SpaceGrotesk-Medium.ttf \
      SpaceGrotesk-Medium.ttf
fetch https://github.com/JetBrains/JetBrainsMono/raw/master/fonts/ttf/JetBrainsMono-Regular.ttf \
      JetBrainsMono-Regular.ttf

for f in SpaceGrotesk-Medium.ttf JetBrainsMono-Regular.ttf; do
    # A truncated or HTML-error download is worse than a missing file, because
    # BinaryData will happily embed it and the plugin will fall back silently.
    head -c 4 "$f" | grep -q $'\x00\x01\x00\x00' || { echo "!! $f is not a TTF"; exit 1; }
done
echo "fonts ok"
