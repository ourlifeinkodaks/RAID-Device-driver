#!/bin/sh


sh make-mirror-test.sh


dd if=/dev/zero bs=512 count=10 | tr '\0' 'A' > mirror/disk1.img
dd if=/dev/zero bs=512 count=10 | tr '\0' 'B' > mirror/disk2.img
dd if=/dev/zero bs=512 count=10 | tr '\0' 'C' > mirror/replace_disk.img

./mirror-test mirror/disk1.img mirror/disk2.img