#!/bin/bash

CURRENT=$(cd $(dirname $0) && pwd)

usage_exit() {
        echo "Usage: $0 [-w width] [-h height] item ..." 1>&2
        exit 1
}

CAM_WIDTH=1024
CAM_HEIGHT=1024
BITRATE=8000000
RENDER_WIDTH=1440
RENDER_HEIGHT=720
BACKGROUND=false
STEREO=
MODE=
FPS=30

while getopts w:h:W:H:BsCEFf: OPT
do
    case $OPT in
        w)  CAM_WIDTH=$OPTARG
            ;;
        h)  CAM_HEIGHT=$OPTARG
            ;;
        W)  RENDER_WIDTH=$OPTARG
            ;;
        H)  RENDER_HEIGHT=$OPTARG
            ;;
        B)  BACKGROUND=true
            ;;
        s)  STEREO="-s"
            ;;
        C)  MODE="-C"
            ;;
        E)  MODE="-E"
            ;;
        F)  MODE="-F"
            ;;
        f)  FPS=$OPTARG
            ;;
        \?) usage_exit
            ;;
    esac
done

if [ ! -e cam0 ]; then
	mkfifo cam0
fi

if [ ! -e cmd ]; then
	mkfifo cmd
fi

raspivid -n -t 0 -w $CAM_WIDTH -h $CAM_HEIGHT -ih -b $BITRATE -fps $FPS -o - > cam0 &
if [ $BACKGROUND = true ]; then
	sudo $CURRENT/picam360-capture.bin -w $CAM_WIDTH -h $CAM_HEIGHT -W $RENDER_WIDTH -H $RENDER_HEIGHT $MODE $STEREO < cmd &
else
	sudo $CURRENT/picam360-capture.bin -w $CAM_WIDTH -h $CAM_HEIGHT -W $RENDER_WIDTH -H $RENDER_HEIGHT $MODE $STEREO -p
fi