#!/bin/sh

qemu-img create -f raw disk.img 120M
mkfs.ext2 -F disk.img
sudo mkdir -p /mnt/rootfs
sudo mount -o loop disk.img /mnt/rootfs
sudo mkdir /mnt/rootfs/boot
sudo cp /boot/vmlinuz /mnt/rootfs/boot/.
sudo cp /boot/initrd.img /mnt/rootfs/boot/.
sudo umount /mnt/rootfs
