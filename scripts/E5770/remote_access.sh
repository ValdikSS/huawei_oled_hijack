#!/system/bin/busybox sh

CONF_FILE="/data/userdata/remote_access"
CURRENT_MODE="$(cat $CONF_FILE)"
TABLE_NAME="SERVICE_INPUT"
echo $CURRENT_MODE

enable_all () {
    xtables-multi iptables -D "$TABLE_NAME" -p tcp --dport 80 -j REJECT
    xtables-multi iptables -D "$TABLE_NAME" -p tcp --dport 23 -j REJECT
    xtables-multi iptables -D "$TABLE_NAME" -p tcp --dport 5555 -j REJECT
    xtables-multi ip6tables -D "$TABLE_NAME" -p tcp --dport 80 -j REJECT
    return 0
}

disable_web () {
    xtables-multi iptables -I "$TABLE_NAME" -p tcp --dport 80 -j REJECT
    xtables-multi ip6tables -I "$TABLE_NAME" -p tcp --dport 80 -j REJECT
    return 0
}

disable_telnet () {
    xtables-multi iptables -I "$TABLE_NAME" -p tcp --dport 23 -j REJECT
    return 0
}

disable_adb () {
    xtables-multi iptables -I "$TABLE_NAME" -p tcp --dport 5555 -j REJECT
    return 0
}

handle_state () {
    [[ "$CURRENT_MODE" == "0" ]] || [[ "$CURRENT_MODE" == "" ]] && enable_all && disable_telnet && disable_adb
    [[ "$CURRENT_MODE" == "1" ]] && enable_all && disable_adb
    [[ "$CURRENT_MODE" == "2" ]] && enable_all
    [[ "$CURRENT_MODE" == "3" ]] && enable_all && disable_web
    [[ "$CURRENT_MODE" == "4" ]] && enable_all && disable_telnet && disable_adb && disable_web
    return 0
}

if [[ "$1" == "boot" ]]
then
    [[ "$CURRENT_MODE" == "" ]] && CURRENT_MODE="0" && echo "0" > $CONF_FILE
    handle_state
    exit 0
fi

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "0" ]] && exit 0
    [[ "$CURRENT_MODE" == "1" ]] && exit 1
    [[ "$CURRENT_MODE" == "2" ]] && exit 2
    [[ "$CURRENT_MODE" == "3" ]] && exit 3
    [[ "$CURRENT_MODE" == "4" ]] && exit 4

    # assuming it's not configured yet, all ports open
    exit 0
fi

if [[ "$1" == "set_next" ]]
then
    if [[ "$CURRENT_MODE" == "0" ]] || [[ "$CURRENT_MODE" == "" ]]; then
        CURRENT_MODE="1" && echo "1" > $CONF_FILE
    elif [[ "$CURRENT_MODE" == "1" ]]; then
        CURRENT_MODE="2" && echo "2" > $CONF_FILE
    elif [[ "$CURRENT_MODE" == "2" ]]; then
        CURRENT_MODE="3" && echo "3" > $CONF_FILE
    elif [[ "$CURRENT_MODE" == "3" ]]; then
        CURRENT_MODE="4" && echo "4" > $CONF_FILE
    elif [[ "$CURRENT_MODE" == "4" ]]; then
        CURRENT_MODE="0" && echo "0" > $CONF_FILE
    fi

    handle_state
fi
