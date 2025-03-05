#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# Script to build an EFI thing suitable for booting with QEMU, possibly running
# it also.

# This just an example. It assumes that

# - you build U-Boot in ${ubdir}/<name> where <name> is the U-Boot board config
# - /mnt/x is a directory used for mounting
# - you have access to the 'pure UEFI' builds for QEMU
#
# UEFI binaries for QEMU used for testing this script:
#
# OVMF-pure-efi.i386.fd at
# https://drive.google.com/file/d/1jWzOAZfQqMmS2_dAK2G518GhIgj9r2RY/view?usp=sharing

# OVMF-pure-efi.x64.fd at
# https://drive.google.com/file/d/1c39YI9QtpByGQ4V0UNNQtGqttEzS-eFV/view?usp=sharing

bzimage_fname=/tmp/kernel/arch/x86/boot/bzImage
shim_fname=/scratch/sglass/shim/shimx64.efi

set -e

usage() {
	echo "Usage: $0 [-a | -p] [other opts]" 1>&2
	echo 1>&2
	echo "   -a   - Package up the app" 1>&2
	echo "   -k   - Add a kernel" 1>&2
	echo "   -o   - Use old EFI app build (before 32/64 split)" 1>&2
	echo "   -p   - Package up the payload" 1>&2
	echo "   -P   - Create a partition table" 1>&2
	echo "   -r   - Run QEMU with the image" 1>&2
	echo "   -s   - Run QEMU with serial only (no display)" 1>&2
	echo "   -S   - Add Shim to run before U-Boot" 1>&2
	echo "   -w   - Use word version (32-bit)" 1>&2
	exit 1
}

# 32- or 64-bit EFI
bitness=64

# app or payload ?
type=app

# create a partition table and put the filesystem in that (otherwise put the
# filesystem in the raw device)
part=

# run the image with QEMU
run=

# run QEMU without a display (U-Boot must be set to stdout=serial)
serial=

# before the 32/64 split of the app
old=

# package up a kernel as well
kernel=

# run shim
shim=

# Set ubdir to the build directory where you build U-Boot out-of-tree
# We avoid in-tree build because it gets confusing trying different builds
ubdir=/tmp/b/

while getopts "akopPrsSw" opt; do
	case "${opt}" in
	a)
		type=app
		;;
	p)
		type=payload
		;;
	k)
		kernel=1
		;;
	r)
		run=1
		;;
	s)
		serial=1
		;;
	w)
		bitness=32
		;;
	o)
		old=1
		;;
	P)
		part=1
		;;
	S)
		shim=1
		;;
	*)
		usage
		;;
	esac
done

run_qemu() {
	extra=
	ssh_port=5555
	if [[ "${bitness}" = "64" ]]; then
		qemu=qemu-system-x86_64
		#bios=OVMF-pure-efi.x64.fd
# 		bios=/usr/share/OVMF/OVMF_CODE_4M.secboot.fd
		#bios=OVMF_CODE_4M.secboot.fd
		bios=/usr/share/OVMF/OVMF_CODE_4M.secboot.fd
		vars=OVMF_VARS_4M.ms.fd
	else
		qemu=qemu-system-i386
		bios=OVMF-pure-efi.i386.fd
	fi
	if [[ -n "${serial}" ]]; then
		extra="-display none -serial mon:stdio"
	else
		extra="-serial mon:stdio"
	fi
	echo "Running ${qemu}"
	# Use 512MB since U-Boot EFI likes to have 256MB to play with
	# -bios "${bios}" \
	# https://wiki.debian.org/SecureBoot/VirtualMachine
	# -net nic,model=virtio -net user,hostfwd=tcp::${ssh_port}-:22
	#-global driver=cfi.pflash01,property=secure,value=on \
	"${qemu}" \
		-m 512 \
	        -enable-kvm \
		-object rng-random,filename=/dev/urandom,id=rng0 \
	        -device virtio-rng-pci,rng=rng0  \
		-drive if=pflash,format=raw,unit=0,file="${bios}",readonly=on \
		-drive if=pflash,format=raw,unit=1,file="${vars}" \
		-machine q35,smm=on \
		-drive id=disk,file="${IMG}",if=none,format=raw \
		-nic none -device ahci,id=ahci \
		-device ide-hd,drive=disk,bus=ahci.0 ${extra}
		#-cdrom /vid/software/linux/ubuntu/ubuntu-24.04.1-desktop-amd64.iso
}

setup_files() {
	local fname

	echo "Packaging ${BUILD}"
	mkdir -p $TMP
	if [[ -n "${shim}" ]]; then
		echo "fs0:\\BOOT\\EFI\\SHIMX64.EFI" >$TMP/startup.nsh
	else
		echo "fs0:u-boot-${type}.efi" >$TMP/startup.nsh
		sudo cp ${ubdir}/${BUILD}/u-boot-${type}.efi $TMP
	fi

	# Can copy in other files here:
	#sudo cp ${ubdir}/$BUILD/image.bin $TMP/chromeos.rom
	#sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz
}

# Copy files into the filesystem
copy_files() {
	sudo cp $TMP/* $MNT
	if [[ -n "${kernel}" ]]; then
		sudo cp ${bzimage_fname} $MNT/vmlinuz
	fi
	if [[ -n "${shim}" ]]; then
		sudo mkdir -p $MNT/BOOT/EFI
		sudo cp ${shim_fname} $MNT/BOOT/EFI/SHIMX64.EFI
# 		sudo cp shimx64.efi $MNT/BOOT/EFI/SHIMX64.EFI
		sudo cp ${ubdir}/${BUILD}/u-boot-${type}.efi $MNT/BOOT/EFI/grubx64.efi
		ls $MNT/BOOT/EFI
	fi
}

# Create a filesystem on a raw device and copy in the files
setup_raw() {
	mkfs.vfat "${IMG}" >/dev/null
	sudo mount -o loop "${IMG}" $MNT
	copy_files
	sudo umount $MNT
}

# Create a partition table and put the filesystem in the first partition
# then copy in the files
setup_part() {
	# Create a gpt partition table with one partition
	parted "${IMG}" mklabel gpt 2>/dev/null

	# This doesn't work correctly. It creates:
	# Number  Start   End     Size    File system  Name  Flags
	#  1      1049kB  24.1MB  23.1MB               boot  msftdata
	# Odd if the same is entered interactively it does set the FS type
	parted -s -a optimal -- "${IMG}" mkpart boot fat32 1MiB 23MiB

	# Map this partition to a loop device
	kp="$(sudo kpartx -av ${IMG})"
	read boot_dev<<<$(grep -o 'loop.*p.' <<< "${kp}")
	test "${boot_dev}"
	dev="/dev/mapper/${boot_dev}"

	mkfs.vfat "${dev}" >/dev/null

	sudo mount -o loop "${dev}" $MNT

	copy_files

	# Sync here since this makes kpartx more likely to work the first time
	sync
	sudo umount $MNT

	# For some reason this needs a sleep or it sometimes fails, if it was
	# run recently (in the last few seconds)
	if ! sudo kpartx -d "${IMG}" > /dev/null; then
		sleep .5
		sudo kpartx -d "${IMG}" > /dev/null || \
			echo "Failed to remove ${boot_dev}, use: sudo kpartx -d ${IMG}"
	fi
}

TMP="/tmp/efi${bitness}${type}"
MNT=/mnt/x
BUILD="efi-x86_${type}${bitness}"
IMG=try.img

if [[ -n "${old}" && "${bitness}" = "32" ]]; then
	BUILD="efi-x86_${type}"
fi

setup_files

qemu-img create "${IMG}" 24M >/dev/null

if [[ -n "${part}" ]]; then
	setup_part
else
	setup_raw
fi

if [[ -n "${run}" ]]; then
	run_qemu
fi
