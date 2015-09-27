#!/bin/bash

cmd_help() {
    echo " - stop, pause, playpause, random"
    echo "   work as expected"
    echo " - play [n]"
    echo "   starts playing current [or n-th] song"
    echo " - next [n], prev [n]"
    echo "   go [n] or one song back or forth"
    echo " - die"
    echo "   quits madasul"
    echo " - status [format]"
    echo "   Prints current status to stdout"
    echo "   Format:"
    echo "   #c: current track, ##: number of tracks"
    echo "   \$p: path, \$g: genre, \$a: artist, \$l: album, #n: number, \$t: title"
    echo " - registerhandler handler"
    echo "   Register a new playing handler"
    echo "   type[ type type...],outbuf,command %s"
    echo "   where outbuf is the file descriptor the handler uses (usually 1 or 2)"
    echo "   example:"
    echo "   registerhandler mp2 mp3,2,mpg123 %s"
    echo "   registers a handler for mp2 and mp3 files, using mpg123"
    echo " - loadlib file"
    echo "   Loads a list of files. The list is tab-seperated:"
    echo "   type	path   artist  album   title   date    genre"
    echo " - showlib [format]"
    echo "   Prints current library to stdout"
    echo "   Format:"
    echo "   #i: track number, ##: number of tracks"
    echo "   \$p: path, \$g: genre, \$a: artist, \$l: album, #n: number, \$t: title"
    echo " - setlist playlist"
    echo "   playlist is a comma seperated list of track numbers"
    echo " - showlist"
    echo "   Print current playlist to stdout"
    echo " - sethook hook command"
    echo "   Sets a hook."
    echo "   Variables in command:"
    echo "   #c: current track, ##: number of tracks"
    echo "   \$f: path, \$g: genre, \$a: artist, \$l: album, \$t: title"
    echo "   \$y: type, #p: port, #ri: PID"
    echo "   Every command is also a hook, also available:"
    echo "   play_before, play_after, play_fail, scout, play_stop, registerhandler_fail, loadlib_fail, setlist_fail, pause, unpause"
}

for i in $(seq 1 2 ${#@}); do
    case ${@:$i:1} in
        -p) let i++; port="${@:$i:1}" ;;
        -h) let i++; host="${@:$i:1}" ;;
        -*) extravar "${@:$i:1}" "${@:$i:1}" ;;
        *) cmd="${@:$i}"; break ;;
    esac
done

if [ "$port" == "" ]; then
    if [[ "$MADAPORT" =~ ^[0-9]+$ ]]; then
        port=$MADAPORT
    else
        port=6666
    fi
fi
if [ "$host" == "" ]; then
    if [[ ! "$MADAHOST" == "" ]]; then
        host="$MADAHOST"
    else
        host="127.0.0.1"
    fi
fi
