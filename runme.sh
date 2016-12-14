#!/bin/bash

make clean 2> /dev/null
make all
./make_initramfs.sh initrd initrd.img 2> /dev/null
qemu-system-x86_64 -kernel kernel -serial stdio -no-hpet -no-acpi -s -initrd initrd.img
make clean 2> /dev/null



