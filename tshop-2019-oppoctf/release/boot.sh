#!/bin/sh
qemu-system-x86_64 -kernel bzImage -nographic -append "rdinit=/linuxrc console=ttyS0 oops=panic panic=1 quiet" -m 128M -cpu qemu64,smap,smep -initrd initramfs.img -smp cores=1,threads=1 2>/dev/null -s
