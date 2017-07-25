#!/system/bin/busybox sh

NETWORK_AUTO='AT^SYSCFGEX="00",3FFFFFFF,2,2,800C5,,'
NETWORK_GSM_ONLY='AT^SYSCFGEX="01",3FFFFFFF,2,2,800C5,,'
NETWORK_UMTS_ONLY='AT^SYSCFGEX="02",3FFFFFFF,2,2,800C5,,'
NETWORK_LTE_ONLY='AT^SYSCFGEX="03",3FFFFFFF,2,2,800C5,,'
NETWORK_LTE_UMTS='AT^SYSCFGEX="0302",3FFFFFFF,2,2,800C5,,'
NETWORK_LTE_GSM='AT^SYSCFGEX="0301",3FFFFFFF,2,2,800C5,,'
NETWORK_UMTS_GSM='AT^SYSCFGEX="0201",3FFFFFFF,2,2,800C5,,'

ATC="/app/bin/oled_hijack/atc.sh"

# Mode caching to prevent menu slowdowns
if [ ! -f "/var/radio_mode" ]
then
    CURRENT_MODE=$($ATC 'AT^SYSCFGEX?' | grep 'SYSCFGEX' | sed 's/^[^"]*"\([^"]*\)".*/\1/')
    echo $CURRENT_MODE > /var/radio_mode
else
    CURRENT_MODE=$(cat /var/radio_mode)
fi

echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "00" ]]   && exit 0
    [[ "$CURRENT_MODE" == "01" ]]   && exit 1
    [[ "$CURRENT_MODE" == "02" ]]   && exit 2
    [[ "$CURRENT_MODE" == "03" ]]   && exit 3
    [[ "$CURRENT_MODE" == "0302" ]] && exit 4
    [[ "$CURRENT_MODE" == "0301" ]] && exit 5
    [[ "$CURRENT_MODE" == "0201" ]] && exit 6
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_MODE" == "00" ]]   && echo -e "$NETWORK_GSM_ONLY\r" > /dev/appvcom && echo 01 > /var/radio_mode
    [[ "$CURRENT_MODE" == "01" ]]   && echo -e "$NETWORK_UMTS_ONLY\r" > /dev/appvcom && echo 02 > /var/radio_mode
    [[ "$CURRENT_MODE" == "02" ]]   && echo -e "$NETWORK_LTE_ONLY\r" > /dev/appvcom && echo 03 > /var/radio_mode
    [[ "$CURRENT_MODE" == "03" ]]   && echo -e "$NETWORK_LTE_UMTS\r" > /dev/appvcom && echo 0302 > /var/radio_mode
    [[ "$CURRENT_MODE" == "0302" ]] && echo -e "$NETWORK_LTE_GSM\r" > /dev/appvcom && echo 0301 > /var/radio_mode
    [[ "$CURRENT_MODE" == "0301" ]] && echo -e "$NETWORK_UMTS_GSM\r" > /dev/appvcom && echo 0201 > /var/radio_mode
    [[ "$CURRENT_MODE" == "0201" ]] && echo -e "$NETWORK_AUTO\r" > /dev/appvcom && echo 00 > /var/radio_mode
    /system/bin/sleep 3
fi

