#!/bin/sh


sh make-stripe-test.sh

for i in 1 2 3 4 5 6; do
    disk="stripe/disk$i.img"
    disks="$disks $disk"
    dd if=/dev/zero bs=512 count=1024 | tr '\0' $i > $disk
done

./stripe-test 2 $disks
./stripe-test 3 $disks
./stripe-test 5 $disks
./stripe-test 7 $disks
./stripe-test 19 $disks
./stripe-test 64 $disks

valgrind --leak-check=full --show-reachable=yes ./stripe-test 32 $disks