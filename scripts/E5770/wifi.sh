#!/system/bin/busybox sh

CONF_FILE="/data/userdata/wifi/config.xml"
grep -q '<wifienable>0</wifienable>' $CONF_FILE
CURRENT_MODE="$?"
echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "0" ]]  && exit 0
    [[ "$CURRENT_MODE" == "1" ]]  && exit 1

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_MODE" == "0" ]] && sed -i 's~<wifienable>0</wifienable>~<wifienable>1</wifienable>~g' $CONF_FILE
    [[ "$CURRENT_MODE" == "1" ]] && sed -i 's~<wifienable>1</wifienable>~<wifienable>0</wifienable>~g' $CONF_FILE
    reboot
fi 
