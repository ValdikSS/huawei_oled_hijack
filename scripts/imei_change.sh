#!/system/bin/busybox sh

IMEI_SET_COMMAND='AT^CIMEI'

ATC="/app/bin/oled_hijack/atc.sh"
HUAWEICALC="/app/bin/oled_hijack/huaweicalc"
IMEI_GENERATOR="/app/bin/oled_hijack/imei_generator"

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
    exit -1
fi

CURRENT_IMEI_CUT="$(echo $CURRENT_IMEI | cut -c 1-4)"
echo $CURRENT_IMEI_CUT

if [[ "$CURRENT_IMEI_CUT" == "8606" ]]
then
    echo $CURRENT_IMEI > /online/imei_backup
fi

echo $CURRENT_IMEI

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_IMEI_CUT" == "8606" ]] && exit 0
    [[ "$CURRENT_IMEI_CUT" == "3542" ]] && exit 1
    [[ "$CURRENT_IMEI_CUT" == "3536" ]] && exit 2
fi

if [[ "$1" == "set_next" ]]
then
    DATALOCK_CODE="$($HUAWEICALC -3 $CURRENT_IMEI)"
    if [[ "$DATALOCK_CODE" != "" ]]
    then
        echo -e "AT^DATALOCK=\"$DATALOCK_CODE\"\r" > /dev/appvcom
        echo -e "Datalock:" "AT^DATALOCK=\"$DATALOCK_CODE\"\r"
    else
        exit -2
    fi
    
    IMEI_ANDROID="$($IMEI_GENERATOR -m 35428207)"
    IMEI_WINPHONE="$($IMEI_GENERATOR -m 35365206)"
    IMEI_BACKUP="$(cat /online/imei_backup)"
    if [[ "$IMEI_BACKUP" == "" ]]
    then
        exit -3
    fi
    #exit 0
    [[ "$CURRENT_IMEI_CUT" == "8606" ]] && echo -e "$IMEI_SET_COMMAND=\"$IMEI_ANDROID\"\r" > /dev/appvcom && echo $IMEI_ANDROID > /var/current_imei
    [[ "$CURRENT_IMEI_CUT" == "3542" ]] && echo -e "$IMEI_SET_COMMAND=\"$IMEI_WINPHONE\"\r" > /dev/appvcom && echo $IMEI_WINPHONE > /var/current_imei
    [[ "$CURRENT_IMEI_CUT" == "3536" ]] && echo -e "$IMEI_SET_COMMAND=\"$IMEI_BACKUP\"\r" > /dev/appvcom && echo $IMEI_BACKUP > /var/current_imei
    
fi

