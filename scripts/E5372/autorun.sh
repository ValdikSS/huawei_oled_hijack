#!/system/bin/busybox sh

/app/bin/oled_hijack/remote_access.sh boot
# force "get" to cache some values
/app/bin/oled_hijack/imei_change.sh get
/app/bin/oled_hijack/no_battery.sh get
/app/bin/oled_hijack/usb_mode.sh get

# Enable IPv6
atc 'at^nvwr=8514,4,01 04 00 00'
