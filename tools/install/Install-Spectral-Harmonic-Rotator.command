#!/bin/bash
# Installs the Spectral Harmonic Rotator Audio Unit for all users, in /Library/Audio/Plug-Ins/Components (Logic loads AU only). Does not install the VST3.
# Asks for the Mac password once. It touches only: the folder this file is in, the copy of Spectral Harmonic Rotator.component in the folder above,
# an old copy of the same name in ~/Library/Audio/Plug-Ins/Components if there is one, and any copy under the plugin's old name
# "Unofficial SMR.component" in either Components folder. The old copy must go: it has the same AU codes (aufx Smru Ptdt), and two
# components with the same codes clash in Logic.
cd "$(dirname "$0")" || exit 1
SRC="$(pwd)"
NAME="Spectral Harmonic Rotator.component"
OLDNAME="Unofficial SMR.component"
DEST="/Library/Audio/Plug-Ins/Components"
USERDIR="$HOME/Library/Audio/Plug-Ins/Components"

if [ ! -d "$SRC/$NAME" ]; then
  echo "Cannot find '$NAME' next to this file. Leave this file in the unzipped Spectral-Harmonic-Rotator folder."
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

xattr -dr com.apple.quarantine "$SRC" 2>/dev/null
echo "Your Mac password is needed to write to $DEST (nothing shows while you type)."
rm -rf "$USERDIR/$NAME" "$USERDIR/$OLDNAME"
sudo rm -rf "$DEST/$NAME" "$DEST/$OLDNAME"
sudo mkdir -p "$DEST"
sudo cp -R "$SRC/$NAME" "$DEST/"
killall -9 AudioComponentRegistrar 2>/dev/null

echo
for d in "$DEST" "$USERDIR"; do
  [ -d "$d/$OLDNAME" ] && echo "WARNING: an old copy is still there: $d/$OLDNAME"
done
if [ -d "$DEST/$NAME" ]; then
  echo "Installed: $DEST/$NAME"
  echo "Now open Logic. If it does not appear, Plug-in Manager, Reset & Rescan Selection."
else
  echo "Install FAILED: the component is not in $DEST"
fi
echo
read -n 1 -s -r -p "Press any key to close."
