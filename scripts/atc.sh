#!/system/bin/busybox sh
ln -s /dev/appvcom /dev/appvcom1 2> /dev/null
killall -STOP ats
/app/bin/oled_hijack/atc "$@"
echo -en "AT^CURC=1\r" > /dev/appvcom1
killall -CONT ats
