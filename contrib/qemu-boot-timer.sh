#/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later

# .. code-block:: bash
#    qemu-boot-timer.sh
#
# Runs qboot (or builds and starts U-Boot) and measures how long it takes to
# start Linux. Must be run in the U-Boot source directory.

# You will need to first install tracing tools, e.g.:
#
# sudo apt install linux-tools-common linux-tools-generic linux-tools-`uname -r`
#
# and also change files and directories below
#
# Example usage to test qboot:

# $ ./contrib/qemu-boot-timer.sh
# qemu-system-x86_64: terminating on signal 15 from pid 3548708 (bash)
# 1) pid 3548829
#  qemu_init_end: 48.352996
#  fw_start: 48.480586 (+0.12759)
#  linux_start_boot: 66.16271 (+17.682124)
#  Exit point 0: 66.37544 (+0.21273)
#
# That shows a time of 17.7 milliseconds for qboot itself

# Enable this to boot qboot instead of U-Boot
#QBOOT=-QB

# Linux version to boot (read from /boot)
version=6.8.0-60-generic

# UUID of root disk within your disk image, which must be called root.img
uuid=bcfdda4a-8249-4f40-9f0f-7c1a76b6cbe8

# Set up directories to use
DIR=/scratch/sglass/qemu-boot-time/
PERF_DATA=perf.data
export PERF_EXEC_PATH=/scratch/sglass/linux/tools/perf

# No settings below here:

# Turn on tracing in Linux
sudo bash <<EOF
echo 1 > /sys/kernel/debug/tracing/events/kvm/enable
echo -1 > /proc/sys/kernel/perf_event_paranoid
mount -o remount,mode=755 /sys/kernel/debug
mount -o remount,mode=755 /sys/kernel/debug/tracing
EOF

# Start recording
sleep 1
cd $DIR
sudo perf record -a -e kvm:kvm_entry -e kvm:kvm_pio -e sched:sched_process_exec \
	-o $PERF_DATA 2> /tmp/trace.log &
PERF_PID=$!

# Run QEWU with qboot or U-Boot (builds U-Boot automatically)
sleep 1
cd ~/u
./scripts/build-qemu -a x86 ${QBOOT} -rs -v -K /boot/vmlinuz-${version} \
	-k -d root.img -I /boot/initrd.img-${version} -C -U ${uuid} -s \
	2>&1 >/tmp/qemu.log &

while [[ -z "$QEMU_PID" ]]; do
	QEMU_PID=$(pgrep qemu-system-x86)
done

# Wait for it to boot
sleep 5

# Kill QEMU and the perf process
kill $QEMU_PID
sleep 1

kill $PERF_PID
wait $PERF_PID

# Adjust permissions on perf data
cd $DIR
sudo chmod a+r $PERF_DATA

# Generate the Python script to permit access to the trace
perf script -g python 2>/dev/null

# Run our script which looks for timing points and prints the results
perf script -s ~/perf-script.py -s ${DIR}/perf-script/qemu-perf-script.py \
	-i $PERF_DATA
