#!/system/bin/busybox sh
ln -s /dev/appvcom /dev/appvcom1
killall -STOP ats
/app/bin/oled_hijack/atc "$@"
killall -CONT ats

