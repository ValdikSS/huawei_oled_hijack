#!/system/bin/busybox sh

IMEI_SET_COMMAND='AT^CIMEI'

ATC="/app/bin/oled_hijack/atc.sh"
HUAWEICALC="/app/bin/oled_hijack/huaweicalc"
IMEI_GENERATOR="/app/bin/oled_hijack/imei_generator"

dataunlock () {
    DATALOCK_CODE="$($HUAWEICALC -3 $CURRENT_IMEI)"
    if [[ "$DATALOCK_CODE" != "" ]]
    then
        echo -e "AT^DATALOCK=\"$DATALOCK_CODE\"\r" > /dev/appvcom
        echo -e "Datalock:" "AT^DATALOCK=\"$DATALOCK_CODE\"\r"
    else
        exit 254
    fi
}

# IMEI caching to prevent menu slowdowns
if [ ! -f "/var/current_imei" ]
then
    CURRENT_IMEI=$($ATC 'AT+CGSN' | grep -o '[0-9]\{15\}')
    echo $CURRENT_IMEI > /var/current_imei
else
    CURRENT_IMEI=$(cat /var/current_imei)
fi

if [[ "$CURRENT_IMEI" == "" ]]
then
    CURRENT_IMEI=$($ATC 'AT+CGSN' | grep -o '[0-9]\{15\}')
    echo $CURRENT_IMEI > /var/current_imei
fi

if [[ "$CURRENT_IMEI" == "" ]]
then
    exit 255
fi

CURRENT_IMEI_CUT="$(echo $CURRENT_IMEI | cut -c 1-8)"

echo $CURRENT_IMEI

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_IMEI_CUT" == "35428207" ]] && exit 1
    [[ "$CURRENT_IMEI_CUT" == "35365206" ]] && exit 2

    # special case for factory imei
    echo $CURRENT_IMEI > /online/imei_backup
    exit 0
fi

if [[ "$1" == "dataunlock" ]]
then
    dataunlock
    exit 0
fi

if [[ "$1" == "set_next" ]]
then
    dataunlock

    IMEI_ANDROID="$($IMEI_GENERATOR -m 35428207)"
    IMEI_WINPHONE="$($IMEI_GENERATOR -m 35365206)"
    IMEI_BACKUP="$(cat /online/imei_backup)"
    if [[ "$IMEI_BACKUP" == "" ]]
    then
        exit 253
    fi

    [[ "$CURRENT_IMEI_CUT" == "35428207" ]] && echo -e "$IMEI_SET_COMMAND=\"$IMEI_WINPHONE\"\r" > /dev/appvcom && echo $IMEI_WINPHONE > /var/current_imei && exit 0
    [[ "$CURRENT_IMEI_CUT" == "35365206" ]] && echo -e "$IMEI_SET_COMMAND=\"$IMEI_BACKUP\"\r" > /dev/appvcom && echo $IMEI_BACKUP > /var/current_imei && exit 0

    # special case for factory imei
    echo -e "$IMEI_SET_COMMAND=\"$IMEI_ANDROID\"\r" > /dev/appvcom && echo $IMEI_ANDROID > /var/current_imei

fi

