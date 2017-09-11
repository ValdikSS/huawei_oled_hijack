#!/system/bin/busybox sh

CONF_FILE="/etc/fix_ttl"
CURRENT_MODE="$(cat $CONF_FILE)"
echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "0" ]] || [[ "$CURRENT_MODE" == "" ]] && exit 0
    [[ "$CURRENT_MODE" == "1" ]]   && exit 1
    [[ "$CURRENT_MODE" == "64" ]]  && exit 1
    [[ "$CURRENT_MODE" == "128" ]] && exit 2
    [[ "$CURRENT_MODE" == "65" ]]  && exit 3

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_MODE" == "0" ]] || [[ "$CURRENT_MODE" == "" ]] && echo "1" > $CONF_FILE
    [[ "$CURRENT_MODE" == "1" ]] || [[ "$CURRENT_MODE" == "64" ]] && echo "128" > $CONF_FILE
    [[ "$CURRENT_MODE" == "128" ]] && echo "65" > $CONF_FILE
    [[ "$CURRENT_MODE" == "65" ]] && echo "0" > $CONF_FILE
fi
