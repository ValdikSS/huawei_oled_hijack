#!/system/bin/busybox sh

CONF_FILE="/data/userdata/adblock"
CURRENT_MODE="$(cat $CONF_FILE)"
echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "" ]]  && exit 0
    [[ "$CURRENT_MODE" == "0" ]] && exit 0
    [[ "$CURRENT_MODE" == "1" ]] && exit 1

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_MODE" == "" ]] || [[ "$CURRENT_MODE" == "0" ]] && echo "1" > $CONF_FILE && /etc/adblock.sh 1
    [[ "$CURRENT_MODE" == "1" ]] && echo "0" > $CONF_FILE && /etc/adblock.sh 0
fi
