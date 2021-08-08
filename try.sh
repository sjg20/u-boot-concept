#!/bin/bash

set -e

file=mmc.img
mnt=/mnt/x
fat=/mnt/y
dstdir=$fat/boot/extlinux

old() {
	mkfs.vfat $file
	sudo mount -o loop $file $mnt
	#sudo cp /tmp/b/efi-x86_payload64/u-boot-payload.efi /mnt/x
	#sudo cp /tmp/efi64/startup.nsh  $mnt
	#sudo cp /tmp/efi64/vmlinuz $mnt
	echo >>/tmp/extlinux.conf <<EOF
ui menu.c32

menu autoboot Arch Boot. Automatic boot in # second{,s}. Press a key for options.
menu title Arch Boot Options.
menu hidden

timeout 50

default Arch

label Arch
    kernel /zImage
    append root=/dev/mmcblk0p2 rw rootfstype=ext4 rootwait consoleblank=0 no_console_suspend=1 console=ttymxc0,115200n8
    fdtdir /dtbs
    initrd /initramfs-linux.img
EOF
	sudo cp /tmp/extlinux.conf $mnt
	ls $mnt
	sudo umount $mnt
}

new() {
	echo 'type=c' | sudo sfdisk $file
	#sudo kpartx -a $file
	loop=$(sudo losetup --show -f -P $file)
	echo "Mounted to $loop"
	fatpart="${loop}p1"
 	sudo mkfs.vfat $fatpart
	sudo mount -o loop ${fatpart} $fat

	cat >/tmp/extlinux.conf << EOF
ui menu.c32

menu autoboot Arch Boot. Automatic boot in # second{,s}. Press a key for options.
menu title Arch Boot Options.
menu hidden

timeout 50

default Arch

label Arch
    kernel /zImage
    append root=/dev/mmcblk0p2 rw rootfstype=ext4 rootwait consoleblank=0 no_console_suspend=1 console=ttymxc0,115200n8
    fdtdir /dtbs
    initrd /initramfs-linux.img
EOF
	sudo mkdir -p $dstdir
	sudo cp /tmp/extlinux.conf $dstdir
	sudo umount $fat

	losetup -d $loop
}

# Remove old devices
for dev in $(losetup |grep $file | awk '{print $1}'); do
	echo "Remove $dev"
	losetup -d $dev
done

qemu-img create $file 20M
new
