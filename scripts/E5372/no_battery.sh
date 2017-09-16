#!/system/bin/busybox sh

ATC="/app/bin/oled_hijack/atc.sh"
IMEI_CHANGE="/app/bin/oled_hijack/imei_change.sh"
CONF_FILE="/var/battery_status"

# battery status caching to prevent menu slowdowns
if [[ ! -f "$CONF_FILE" ]]
then
    $IMEI_CHANGE dataunlock
    CURRENT_BATTERY="$($ATC 'AT^NVRD=50364' | grep 'NVRD' | grep -o '[0-9 ]\{11\}')"
    if [[ "$CURRENT_BATTERY" == "00 00 00 00" ]]
    then
        CURRENT_BATTERY_STATUS="0"
        echo 0 > $CONF_FILE
    elif [[ "$CURRENT_BATTERY" == "01 01 00 00" ]]
    then
        CURRENT_BATTERY_STATUS="1"
        echo 1 > $CONF_FILE
    else
        # error
        exit 255
    fi
else
    CURRENT_BATTERY_STATUS="$(cat $CONF_FILE)"
fi

if [[ "$CURRENT_BATTERY_STATUS" == "" ]]
then
    exit 254
fi

echo $CURRENT_BATTERY_STATUS

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_BATTERY_STATUS" == "0" ]] && exit 0
    [[ "$CURRENT_BATTERY_STATUS" == "1" ]] && exit 1

    exit 253
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_BATTERY_STATUS" == "0" ]] && echo -e "AT^NVWR=50364,04,01 01 00 00\r" > /dev/appvcom && echo 1 > $CONF_FILE && exit 0
    [[ "$CURRENT_BATTERY_STATUS" == "1" ]] && echo -e "AT^NVWR=50364,04,00 00 00 00\r" > /dev/appvcom && echo 0 > $CONF_FILE && exit 0

    exit 253
fi
