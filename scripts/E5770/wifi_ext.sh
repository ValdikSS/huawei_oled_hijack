#!/system/bin/busybox sh

PIPE="/var/wifihook"
HOOK_CLIENT="/app/bin/oled_hijack/wifi_hook_client"
SET_DISABLED='<?xml version="1.0" encoding="UTF-8"?><request><Handover>0</Handover></request>'
SET_ENABLED='<?xml version="1.0" encoding="UTF-8"?><request><Handover>2</Handover></request>'

function get_state() {
    OUT="$($HOOK_CLIENT handover-setting 1 1)"
    echo $OUT | grep -q ">0<"
    CURRENT_MODE=$?
    echo $CURRENT_MODE
}

function set_state() {
    [[ "$1" == "0" ]] && $HOOK_CLIENT handover-setting 2 "$SET_DISABLED"
    [[ "$1" == "1" ]] && $HOOK_CLIENT handover-setting 2 "$SET_ENABLED"
}


if [[ "$1" == "get" ]]
then
    get_state

    [[ "$CURRENT_MODE" == "0" ]]  && exit 0
    [[ "$CURRENT_MODE" == "1" ]]  && exit 1

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    get_state
    [[ "$CURRENT_MODE" == "0" ]] && set_state 1
    [[ "$CURRENT_MODE" == "1" ]] && set_state 0
fi
