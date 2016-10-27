#!/bin/bash

make clean
make all 
qemu-system-x86_64 -kernel kernel -serial stdio -no-hpet -no-acpi -s



