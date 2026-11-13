#!/usr/bin/env bash
# Copies the Material Symbols (Sharp, weight 300) used by the launcher into
# launcher/assets. Internal names stay stable; only this table maps them.
# Usage: scripts/vendor-material-icons.sh [version]
set -euo pipefail
version="${1:-0.48.3}"
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL "https://registry.npmjs.org/@material-symbols/svg-300/-/svg-300-${version}.tgz" | tar xz -C "$tmp"
src="$tmp/package/sharp"

declare -A ui=(
  [bluetooth]=bluetooth [bluetooth-off]=bluetooth_disabled
  [ethernet]=lan [ethernet-off]=signal_disconnected
  [power]=power_settings_new [search]=search
  [chevron-left]=chevron_left [chevron-right]=chevron_right [chevron-small]=chevron_right
  [link]=link [shield-check]=verified_user [trash]=delete
  [check]=check [x]=close [refresh]=refresh
  [warning-triangle]=warning [error-square]=error
  [gamepad]=sports_esports [remote]=settings_remote [headphones]=headphones
  [keyboard]=keyboard [speaker]=soundbar [mouse]=mouse [phone]=mobile_2
  [device]=devices_other [home]=home [arrows]=open_with
)
declare -A apps=(
  [stream]=smart_display [tv]=tv [android]=android [gamepad]=stadia_controller
  [desktop]=desktop_windows [cast]=cast [music]=music_note [movie]=movie [web]=public
)

rm -f "$root"/launcher/assets/icons/*.svg "$root"/launcher/assets/apps/*.svg
for name in "${!ui[@]}"; do cp "$src/${ui[$name]}.svg" "$root/launcher/assets/icons/$name.svg"; done
for name in "${!apps[@]}"; do cp "$src/${apps[$name]}.svg" "$root/launcher/assets/apps/$name.svg"; done
cp "$tmp/package/LICENSE" "$root/launcher/assets/LICENSE-material-symbols.txt"
echo "Material Symbols ${version}: ${#ui[@]} UI icons, ${#apps[@]} app icons"
