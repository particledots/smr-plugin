#!/bin/bash
# Installs the Unofficial SMR Audio Unit for all users, in /Library/Audio/Plug-Ins/Components (Logic loads AU only). Does not install the VST3.
# Asks for the Mac password once. It touches only: the folder this file is in, the copy of Unofficial SMR.component in the folder above,
# and an old copy of the same name in ~/Library/Audio/Plug-Ins/Components if there is one.
cd "$(dirname "$0")" || exit 1
SRC="$(pwd)"
DEST="/Library/Audio/Plug-Ins/Components"
OLD="$HOME/Library/Audio/Plug-Ins/Components/Unofficial SMR.component"

if [ ! -d "$SRC/Unofficial SMR.component" ]; then
  echo "Cannot find 'Unofficial SMR.component' next to this file. Leave this file in the unzipped Unofficial-SMR folder."
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

xattr -dr com.apple.quarantine "$SRC" 2>/dev/null
echo "Your Mac password is needed to write to $DEST (nothing shows while you type)."
rm -rf "$OLD"
sudo rm -rf "$DEST/Unofficial SMR.component"
sudo mkdir -p "$DEST"
sudo cp -R "$SRC/Unofficial SMR.component" "$DEST/"
killall -9 AudioComponentRegistrar 2>/dev/null

echo
if [ -d "$DEST/Unofficial SMR.component" ]; then
  echo "Installed: $DEST/Unofficial SMR.component"
  echo "Now open Logic. If it does not appear, Plug-in Manager, Reset & Rescan Selection."
else
  echo "Install FAILED: the component is not in $DEST"
fi
echo
read -n 1 -s -r -p "Press any key to close."
