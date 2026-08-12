#!/system/bin/sh
while [ "$(getprop sys.boot_completed | tr -d '\r')" != "1" ]; do sleep 1; done
sleep 1

MODULE_ID=@MODULE_ID@

MODULE_LIB_NAME=@MODULE_LIB_NAME@

ABI=$(getprop ro.product.cpu.abi)

copy_lib() {
  PACKAGE=$1
  PID=$(dumpsys package "$PACKAGE" | grep userId= | sed 's/[[:blank:]]*userId=//g')
  cp "/data/adb/modules/${MODULE_ID}/lib/armeabi-v7a/lib${MODULE_LIB_NAME}.so" /data/data/"$PACKAGE"/armeabi-v7a.so
  chown "$PID":"$PID" /data/data/"$PACKAGE"/armeabi-v7a.so
}

copy_lib64() {
  PACKAGE=$1
  PID=$(dumpsys package "$PACKAGE" | grep userId= | sed 's/[[:blank:]]*userId=//g')
  cp "/data/adb/modules/${MODULE_ID}/lib/arm64-v8a/lib${MODULE_LIB_NAME}.so" /data/data/"$PACKAGE"/arm64-v8a.so
  chown "$PID":"$PID" /data/data/"$PACKAGE"/arm64-v8a.so
  copy_lib "$PACKAGE"
}

get_installed_abi() {
  PACKAGE=$1
  PACKAGE_ABI=$(dumpsys package "$PACKAGE" | grep primaryCpuAbi= | sed 's/[[:blank:]]*primaryCpuAbi=//g')
  echo "$PACKAGE_ABI"
}

if [ "$ABI" = "x86" ]; then
  if [ -d "/data/data/jp.co.cygames.umamusume" ]; then
    copy_lib "jp.co.cygames.umamusume"
  fi
  if [ -d "/data/data/com.kakaogames.umamusume" ]; then
    copy_lib "com.kakaogames.umamusume"
  fi
  if [ -d "/data/data/com.komoe.kmumamusumegp" ]; then
    copy_lib "com.komoe.kmumamusumegp"
  fi
  if [ -d "/data/data/com.komoe.umamusumeofficial" ]; then
    copy_lib "com.komoe.umamusumeofficial"
  fi
elif [ "$ABI" = "x86_64" ]; then
  if [ -d "/data/data/jp.co.cygames.umamusume" ]; then
    INSTALLED_ABI=$(get_installed_abi "jp.co.cygames.umamusume")
    if [ "$INSTALLED_ABI" = "armeabi-v7a" ]; then
      copy_lib "jp.co.cygames.umamusume"
      else
      copy_lib64 "jp.co.cygames.umamusume"
    fi
  fi
  if [ -d "/data/data/com.kakaogames.umamusume" ]; then
    INSTALLED_ABI=$(get_installed_abi "com.kakaogames.umamusume")
    if [ "$INSTALLED_ABI" = "armeabi-v7a" ]; then
      copy_lib "com.kakaogames.umamusume"
      else
      copy_lib64 "com.kakaogames.umamusume"
    fi
  fi
  if [ -d "/data/data/com.komoe.kmumamusumegp" ]; then
    INSTALLED_ABI=$(get_installed_abi "com.komoe.kmumamusumegp")
    if [ "$INSTALLED_ABI" = "armeabi-v7a" ]; then
      copy_lib "com.komoe.kmumamusumegp"
      else
      copy_lib64 "com.komoe.kmumamusumegp"
    fi
  fi
  if [ -d "/data/data/com.komoe.umamusumeofficial" ]; then
    INSTALLED_ABI=$(get_installed_abi "com.komoe.umamusumeofficial")
    if [ "$INSTALLED_ABI" = "armeabi-v7a" ]; then
      copy_lib "com.komoe.umamusumeofficial"
      else
      copy_lib64 "com.komoe.umamusumeofficial"
    fi
  fi
fi
