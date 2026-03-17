enforce_install_from_magisk_app() {
  if $BOOTMODE; then
    ui_print "- Installing from Magisk app"
  else
    ui_print "*********************************************************"
    ui_print "! Install from recovery is NOT supported"
    ui_print "! Some recovery has broken implementations, install with such recovery will finally cause Riru or Riru modules not working"
    ui_print "! Please install from Magisk app"
    abort "*********************************************************"
  fi
}

# Functions from util_functions.sh
enforce_install_from_magisk_app

# Check architecture
if [ "$ARCH" != "arm" ] && [ "$ARCH" != "arm64" ] && [ "$ARCH" != "x86" ] && [ "$ARCH" != "x64" ]; then
  abort "! Unsupported platform: $ARCH"
else
  ui_print "- Device platform: $ARCH"
fi

# Extract libs
ui_print "- Extracting module files"

extract "$ZIPFILE" 'module.prop' "$MODPATH"
extract "$ZIPFILE" 'system.prop' "$MODPATH"
extract "$ZIPFILE" 'uninstall.sh' "$MODPATH"
extract "$ZIPFILE" 'classes.dex' "$MODPATH"

mkdir -p "$MODPATH/zygisk"
if [ "$ARCH" = "arm" ] || [ "$ARCH" = "arm64" ]; then
  extract "$ZIPFILE" "lib/armeabi-v7a/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk/armeabi-v7a.so"

  if [ "$IS64BIT" = true ]; then
    extract "$ZIPFILE" "lib/arm64-v8a/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk" true
    mv "$MODPATH/zygisk/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk/arm64-v8a.so"
  fi
fi

if [ "$ARCH" = "x86" ] || [ "$ARCH" = "x64" ]; then
  extract "$ZIPFILE" "lib/x86_64/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk/x86_64.so"

  if [ "$IS64BIT" = true ]; then
    extract "$ZIPFILE" "lib/x86/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk" true
    mv "$MODPATH/zygisk/lib$MODULE_LIB_NAME.so" "$MODPATH/zygisk/x86.so"
  fi
fi
