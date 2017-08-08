#!/system/bin/busybox sh

ATC="/app/bin/oled_hijack/atc.sh"
CONF_FILE="/var/usb_mode"

MODE_0="A1,A2;12,16,A1,A2"
MODE_1="FF;12,16,A2"
MODE_2="FF;12,16"
MODE_3="FF;12,16,A1,A2,5,A,13,14"

status_from_mode() {
    [[ "$1" == "$MODE_0" ]] && echo 0
    [[ "$1" == "$MODE_1" ]] && echo 1
    [[ "$1" == "$MODE_2" ]] && echo 2
    [[ "$1" == "$MODE_3" ]] && echo 3
}

# usb mode caching to prevent menu slowdowns
if [[ ! -f "$CONF_FILE" ]]
then
    CURRENT_USB_MODE="$($ATC 'AT^SETPORT?' | grep 'SETPORT' | grep -o ':[0-9A-F;,]*' | cut -b 2-99)"
    CURRENT_USB_MODE="$(status_from_mode "$CURRENT_USB_MODE")"
    if [[ "$CURRENT_USB_MODE" == "" ]]
    then
        # error
        exit 255
    fi
    echo $CURRENT_USB_MODE > $CONF_FILE
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

    exit 253
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_USB_MODE" == "0" ]] && echo -e "AT^SETPORT=\"$MODE_1\"\r" > /dev/appvcom && echo 1 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "1" ]] && echo -e "AT^SETPORT=\"$MODE_2\"\r" > /dev/appvcom && echo 2 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "2" ]] && echo -e "AT^SETPORT=\"$MODE_3\"\rAT^NVWR=33,4,2,0,0,0\r" > /dev/appvcom && echo 3 > $CONF_FILE && exit 0
    [[ "$CURRENT_USB_MODE" == "3" ]] && echo -e "AT^SETPORT=\"$MODE_0\"\rAT^NVWR=33,4,0,0,0,0\r" > /dev/appvcom && echo 0 > $CONF_FILE && exit 0

    exit 253
fi
