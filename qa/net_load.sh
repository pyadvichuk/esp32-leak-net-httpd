#!/bin/bash

do_help ()
{
cat << EOF
Usage: $0  [-h] [-v] [-m <transport_mode>] -d <target> -P <threads> -U <threads> -G <threads> -V <threads>

  -v               Print additional info

  -h               Print out this message and exit
EOF
}

TARGET=""
TRANSPORT="http://"
P_THREADS="0"
G_THREADS="0"
V_THREADS="0"
U_THREADS="0"
VERBOSE=""

# get command line options
OLD_OPTIND=$OPTIND
while getopts "hvP:G:V:U:t:m:" in_options
do
    case ${in_options} in
        P) P_THREADS=${OPTARG}; ;;
        G) G_THREADS=${OPTARG}; ;;
        V) V_THREADS=${OPTARG}; ;;
        U) U_THREADS=${OPTARG}; ;;
        m) [ "https" == "${OPTARG}" ] && TRANSPORT="https://"; ;;
        t) TARGET="${OPTARG}";  ;;
        v) VERBOSE="true";      ;;
        h) opts_help='true';    ;;
        \?) opts_error='true';  ;;
    esac
done
shift $((OPTIND-1))
if [ $# -ne 0 ]; then
    opts_error=true
    echo "Invalid command line ending: '$@'"
fi
OPTIND=$OLD_OPTIND
if test $opts_help; then
    do_help
    exit 0
elif test $opts_error; then
    exit 1
fi

URL="${TRANSPORT}${TARGET}"
LOGIN=admin
PASS=wallbox
LOGDIR="./log"

TH_STOP="0"
cleanup ()
{
    TH_STOP="1"
    wait
}
trap cleanup EXIT

func_GET_main_js()
{
    local th="$1"
    local out="$2"
    local iter=1000000

    echo >${LOGDIR}/result_get_${th}.test
    echo >${LOGDIR}/result_get_${th}.err

    for n in $(seq 1 ${iter}) ;
      do
        if [ "${TH_STOP}" == "1" ]; then
            break
        fi
        echo '-------------------' $n >>${LOGDIR}/result_get_${th}.test
        echo '-------------------' $n >>${LOGDIR}/result_get_${th}.err
        dummy=$(curl  -X GET $URL/main.js 2>>${LOGDIR}/result_get_${th}.err | tr -d '\0' | tee ${LOGDIR}/result_get_${th}.test)
        tm="0.$(( ((RANDOM<<15)|RANDOM) % 45 + 15 ))"
        sleep $tm
    done
}

func_PING_target()
{
    local th="$1"
    local out="$2"
    local iter=1000000

    echo >${LOGDIR}/result_ping_${th}.test

    for n in $(seq 1 ${iter}) ;
      do
        if [ "${TH_STOP}" == "1" ]; then
            break
        fi
        sz="$(( ((RANDOM<<15)|RANDOM) % 600 + 200 ))"
        dummy=$(ping ${TARGET} -i 0,4 -s ${sz} -c 20 2>>/dev/null | grep "packets transmitted" | tee -a ${LOGDIR}/result_ping_${th}.test)
        loss=$(echo $dummy | awk '{print $6}')
        [ "${loss}" == "0%" ] || echo "($th): ping: $sz bytes: $loss packet loss" > /proc/$out/fd/1
        tm="0.$(( ((RANDOM<<15)|RANDOM) % 45 + 15 ))"
        sleep $tm
    done
}

func_FLOOD_target()
{
    local th="$1"
    local out="$2"
    local iter=1000000
    local port="123"

    echo >${LOGDIR}/result_flood_${th}.test

    for n in $(seq 1 ${iter}) ;
      do
        if [ "${TH_STOP}" == "1" ]; then
            break
        fi
        pyload=""
        sz="$(( ((RANDOM<<15)|RANDOM) % 1200 + 100 ))"
        for i in $(seq 1 ${sz}) ; do pyload="${pyload}."; done
        echo -n "${pyload}"  > /dev/udp/${TARGET}/${port} 2>>/dev/null | tee -a ${LOGDIR}/result_flood_${th}.test
        tm="0.$(( ((RANDOM<<15)|RANDOM) % 45 + 15 ))"
        sleep $tm
    done
}

rm -rf ${LOGDIR} 2>/dev/null
mkdir -p ${LOGDIR}

echo "mypid $$"

if [ "0" != "${P_THREADS}" ]; then
  echo "> PING "
  for i in `seq 1 ${P_THREADS}` ;
    do
      echo -n "."
      func_PING_target $i $$ &
  done
  echo ""
fi

if [ "0" != "${G_THREADS}" ]; then
  echo "> GET "
  for i in `seq 1 ${G_THREADS}` ;
    do
      echo -n "."
      func_GET_main_js $i $$ &
  done
  echo ""
fi

if [ "0" != "${U_THREADS}" ]; then
  echo "> FLOOD "
  for i in `seq 1 ${U_THREADS}` ;
    do
      echo -n "."
      func_FLOOD_target $i $$ &
  done
  echo ""
fi

wait

