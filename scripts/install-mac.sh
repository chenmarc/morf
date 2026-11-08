#!/usr/bin/env bash
# Installs the morf plugins and app you just built on this Mac.
#
#   scripts/install-mac.sh                    # from the default build folder (build/)
#   scripts/install-mac.sh build-universal    # from another build folder
#   ./install-mac.sh .                        # from an unzipped CI download (bundles next to the script)
#
# It copies the VST3 and AU into the system plug-in folders (the ones Ableton and Logic scan by
# default) and the app into /Applications. It also removes the macOS download-quarantine flag,
# re-signs each bundle and checks the signature. It asks for your password because the system
# folders belong to the administrator.
set -euo pipefail

from="${1:-build}"
if [[ -d "$from/plugin/morf_plugin_artefacts/Release" ]]; then
  artefacts="$from/plugin/morf_plugin_artefacts/Release"
  vst3="$artefacts/VST3/morf.vst3"; au="$artefacts/AU/morf.component"; app="$artefacts/Standalone/morf.app"
elif [[ -d "$from/morf.vst3" || -d "$from/morf.component" || -d "$from/morf.app" ]]; then
  vst3="$from/morf.vst3"; au="$from/morf.component"; app="$from/morf.app"
else
  echo "No morf build found in $from."
  echo "Build first:  cmake --preset default && cmake --build --preset default"
  exit 1
fi

install_bundle() {
  local src="$1" dest_dir="$2"
  local name
  name="$(basename "$src")"
  if [[ ! -d "$src" ]]; then
    echo "skipping $name (not built)"
    return
  fi
  sudo mkdir -p "$dest_dir"
  sudo rm -rf "$dest_dir/$name"
  sudo cp -R "$src" "$dest_dir/"
  sudo xattr -dr com.apple.quarantine "$dest_dir/$name" 2>/dev/null || true
  sudo codesign --force --deep --sign - "$dest_dir/$name"
  codesign --verify --deep --strict "$dest_dir/$name"
  echo "installed $dest_dir/$name ($(lipo -archs "$(find "$dest_dir/$name/Contents/MacOS" -type f | head -1)"))"
}

# Old copies in your personal Library folder would shadow the new ones, so remove them.
for stale in "$HOME/Library/Audio/Plug-Ins/VST3/morf.vst3" "$HOME/Library/Audio/Plug-Ins/Components/morf.component"; do
  if [[ -e "$stale" ]]; then
    echo "removing old copy $stale"
    rm -rf "$stale"
  fi
done

install_bundle "$vst3" /Library/Audio/Plug-Ins/VST3
install_bundle "$au" /Library/Audio/Plug-Ins/Components
install_bundle "$app" /Applications

# Make macOS notice the new Audio Unit straight away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Done. In Ableton Live: quit and reopen Live, then hold Option and click Rescan"
echo "(Preferences > Plug-ins). Option-click forces a full rescan instead of using Live's cache."
