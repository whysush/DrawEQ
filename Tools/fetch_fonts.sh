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
    #
    # Checked with od rather than by grepping for the NUL bytes of the TrueType
    # magic: grep's handling of NUL differs between GNU grep and the one Git
    # Bash ships on Windows, and the check quietly failed there.
    magic=$(od -An -tx1 -N4 "$f" | tr -d ' \n')

    if [ "$magic" != "00010000" ] && [ "$magic" != "74727565" ]; then
        echo "!! $f is not a TrueType file (magic: $magic)"
        exit 1
    fi
done
echo "fonts ok"
