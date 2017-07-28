#!/system/bin/busybox sh

CURRENT_MODE="$(cat /etc/fix_ttl)"
echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "0" ]]   && exit 0
    [[ "$CURRENT_MODE" == "1" ]]   && exit 1
    [[ "$CURRENT_MODE" == "64" ]]  && exit 1
    [[ "$CURRENT_MODE" == "128" ]] && exit 2

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    mount -o remount,rw /system /system
    [[ "$CURRENT_MODE" == "0" ]] && echo "1" > /etc/fix_ttl
    [[ "$CURRENT_MODE" == "1" ]] || [[ "$CURRENT_MODE" == "64" ]] && echo "128" > /etc/fix_ttl
    [[ "$CURRENT_MODE" == "128" ]] && echo "0" > /etc/fix_ttl
    mount -o remount,ro /system /system
fi
