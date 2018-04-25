#!/bin/sh

sh make-raid4-test.sh

while [ "$1" ] ; do
    case $1 in
    -v) verbose=true;;
    -create) create=true;;
    -cmds) cmds=true;;
    esac
    shift
done


for i in 1 2 3 4 5 6 7; do
    disks="$disks raid4/disk$i.img"
done
d4=${disks% *}          


if [ "$create" ] ; then
    echo disks created:
    echo $disks
    exit 1
fi

overall=SUCCESS
if [ "$cmds" ] ; then
    ECHO=echo
fi


for i in "$d4" "$disks"; do
    for j in 4 16 20; do
    sh temp_test.sh 2<&-
    echo testing RAID4 on $(echo $x | wc -w) drives with stripe size $j blocks
    echo ./raid4-test $j $i
    $ECHO ./raid4-test $j $i
    if [ $? -ne 0 ] ; then overall=FAILED; fi
    if [ $overall = FAILED -a "$verbose" ] ; then
        echo failed on:     ./raid4-test $j $i
        exit
    fi
    done
done


echo "Testing for writes past end of volume"
sh temp_test.sh 2<&-

$ECHO ./raid4-test 64 $disks
if [ $? -ne 0 ] ; then overall=FAILED; fi


[ "$cmds" ] || for d in $disks; do
    dd if=$d bs=512 skip=1024 2<&-
done | tr -d '\0' | wc -c | while read bytes; do
    if [ $bytes -gt 0 ] ; then
    echo ERROR: garbage added to end of disks
    overall=FAILED
    fi
done

echo Overall test: $overall
$ECHO rm -f $disks
