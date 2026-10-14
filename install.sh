#!/bin/zsh
# Build ykswitch, install it root-owned, and allow passwordless `sudo ykswitch`
# for the invoking user. The binary must stay root-owned and non-writable by
# the user, or the sudoers rule becomes a passwordless-root hole.
set -euo pipefail
cd "${0:A:h}"

BIN=/usr/local/bin/ykswitch
RULE=/etc/sudoers.d/ykswitch

clang -O2 -o ykswitch ykswitch.c -framework IOKit -framework CoreFoundation
sudo install -d -o root -g wheel -m 755 /usr/local/bin
sudo install -o root -g wheel -m 755 ykswitch "$BIN"

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
echo "$USER ALL=(root) NOPASSWD: $BIN" > "$tmp"
sudo visudo -cqf "$tmp"
sudo install -o root -g wheel -m 440 "$tmp" "$RULE"

echo "installed $BIN; try: sudo -n ykswitch status"
