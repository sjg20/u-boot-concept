#/usr/bin/python

def patch_old():
	inf = open('b/coreboot-x86/out/image.bin')
	badf = open('b/chromebook_link/out/image.bin')
	outf = open('try.bin', 'w')

	data = inf.read()
	bad = badf.read()

	def copybad(start, len):
		global data
		data = data[:start] + bad[start:start + len] + data[start + len:]

	#data = data[:-16] + bad[-16:]
	copybad(0x7ffff0, 16)
	#data = data[:-0x400] + bad[-0x400:]
	start = 0x7ffcd0
	copybad(start, 0x40)

	#data = data[:-0x100000] + bad[-0x100000:]
	print len(data), len(bad)
	#data = bad
	outf.write(data)

inf = open('b/chromebook_link/out/image.bin', 'rw')
mrc = open('arch/x86/dts/mrc.bin')
data = inf.read()
mrc_data = mrc.read()
data = data[:0x7a0000] + mrc_data + data[0x7a0000 + len(mrc_data):]
outf = open('try.bin', 'w')
outf.write(data)
