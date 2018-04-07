#!/system/bin/busybox sh

ATC="/app/bin/oled_hijack/atc"
CONF_FILE="/var/usb_mode"

# HACK: ECM mode needs kernel patch. See /system/etc/patchblocked.sh
# Also see drivers/usb/mbb_usb_unitary/hw_pnp_adapt.{c,h} to learn more

# Stock
MODE_0="01 00 00 00 A1 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A3 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
# RNDIS
MODE_1="01 00 00 00 A3 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A3 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
# ECM. DO NOT SET IT UNLESS THE KERNEL IS PATCHED! USES 'finger' GADGET. THIS IS HACK!
MODE_2="01 00 00 00 0B 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0B 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
# Modem
MODE_3="01 00 00 00 01 03 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 03 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
# Modem (NCM)
MODE_4="01 00 00 00 01 03 02 16 00 00 00 00 00 00 00 00 00 00 00 00 00 01 03 02 16 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
# Debug
MODE_5="01 00 00 00 FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A3 12 A1 A2 05 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"

status_from_mode() {
    [[ "$1" == "$MODE_0" ]] && echo 0
    [[ "$1" == "$MODE_1" ]] && echo 1
    [[ "$1" == "$MODE_2" ]] && echo 2
    [[ "$1" == "$MODE_3" ]] && echo 3
    [[ "$1" == "$MODE_4" ]] && echo 4
    [[ "$1" == "$MODE_5" ]] && echo 5
}

# usb mode caching to prevent menu slowdowns
if [[ ! -f "$CONF_FILE" ]]
then
    CURRENT_USB_MODE="$($ATC 'AT^NVRD=50091' | grep 'NVRD' | grep -o ',[0-9A-F ]\{179\}' | cut -b 2-999)"
    CURRENT_USB_MODE="$(status_from_mode "$CURRENT_USB_MODE")"
    if [[ "$CURRENT_USB_MODE" == "" ]]
    then
        # error
        exit 255
    fi
    echo $CURRENT_USB_MODE > $CONF_FILE
    # Reset USB if ECM is selected and we're booting
    [[ "$1" == "boot" ]] && [[ "$CURRENT_USB_MODE" == "2" ]] && ecall pnp_notify_to_switch
else
    CURRENT_USB_MODE="$(cat $CONF_FILE)"
fi

if [[ "$CURRENT_USB_MODE" == "" ]]
then
    exit 254
fi

echo $CURRENT_USB_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_USB_MODE" == "0" ]] && exit 0
    [[ "$CURRENT_USB_MODE" == "1" ]] && exit 1
    [[ "$CURRENT_USB_MODE" == "2" ]] && exit 2
    [[ "$CURRENT_USB_MODE" == "3" ]] && exit 3
    [[ "$CURRENT_USB_MODE" == "4" ]] && exit 4
    [[ "$CURRENT_USB_MODE" == "5" ]] && exit 5

    exit 253
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_USB_MODE" == "0" ]] && echo -e "AT^NVWR=50091,60,$MODE_1\r" > /dev/appvcom && echo 1 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "1" ]] && echo -e "AT^NVWR=50091,60,"$MODE_2"\r" > /dev/appvcom && echo 2 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "2" ]] && echo -e "AT^NVWR=50091,60,"$MODE_3"\r" > /dev/appvcom && echo 3 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "3" ]] && echo -e "AT^NVWR=50091,60,"$MODE_4"\r" > /dev/appvcom && echo 4 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "4" ]] && echo -e "AT^NVWR=50091,60,"$MODE_5"\rAT^NVWR=33,4,2,0,0,0\r" > /dev/appvcom && echo 5 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "5" ]] && echo -e "AT^NVWR=50091,60,"$MODE_0"\rAT^NVWR=33,4,0,0,0,0\r" > /dev/appvcom && echo 0 > $CONF_FILE && exit 0

    exit 253
fi
