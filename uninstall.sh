#!/bin/zsh
# Re-enable the key and remove everything install.sh added.
set -euo pipefail

BIN=/usr/local/bin/ykswitch

[[ -x $BIN ]] && sudo "$BIN" on
sudo rm -f "$BIN" /etc/sudoers.d/ykswitch /var/run/ykswitch.pid
echo "uninstalled"
