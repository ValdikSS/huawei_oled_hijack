#!/system/bin/busybox sh

CONF_FILE="/data/userdata/dns_over_tls"
CURRENT_MODE="$(cat $CONF_FILE)"
echo $CURRENT_MODE

if [[ "$1" == "get" ]]
then
    [[ "$CURRENT_MODE" == "" ]]  && exit 0
    [[ "$CURRENT_MODE" == "0" ]] && exit 0
    [[ "$CURRENT_MODE" == "1" ]] && exit 1
    [[ "$CURRENT_MODE" == "2" ]] && exit 2

    # error
    exit 255
fi

if [[ "$1" == "set_next" ]]
then
    [[ "$CURRENT_MODE" == "" ]] || [[ "$CURRENT_MODE" == "0" ]] && echo "1" > $CONF_FILE && /etc/dns_over_tls.sh 1
    [[ "$CURRENT_MODE" == "1" ]] && echo "2" > $CONF_FILE && /etc/dns_over_tls.sh 2
    [[ "$CURRENT_MODE" == "2" ]] && echo "0" > $CONF_FILE && /etc/dns_over_tls.sh 0
fi
