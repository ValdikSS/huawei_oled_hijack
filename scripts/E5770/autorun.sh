#!/system/bin/busybox sh

# force "get" to cache some values
/app/bin/oled_hijack/imei_change.sh get
/app/bin/oled_hijack/usb_mode.sh boot
# remote_access.sh is called from /system/bin/iptables-fixttl-wrapper.sh
