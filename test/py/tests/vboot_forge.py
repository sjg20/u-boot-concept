# SPDX-License-Identifier: GPL-2.0+
#
# U-Boot Verified Boot Test

import struct
import binascii
from textwrap import wrap
from io import BytesIO

#
# struct parsing helpers
#

class BetterStructMeta(type):
    def __new__(cls, clsname, superclasses, attributedict):
        if clsname != 'BetterStruct':
            fields = attributedict['__fields__']
            field_types = [ _[0] for _ in fields ]
            field_names = [ _[1] for _ in fields if _[1] is not None]
            attributedict['__names__'] = field_names
            s = struct.Struct(attributedict.get('__endian__', '') + ''.join(field_types))
            attributedict['__struct__'] = s
            attributedict['size'] = s.size
        return type.__new__(cls, clsname, superclasses, attributedict)

class BetterStruct(object, metaclass=BetterStructMeta):
    def __init__(self):
        for t, n in self.__fields__:
            if 's' in t:
                setattr(self, n, '')
            elif t in ('Q', 'I', 'H', 'B'):
                setattr(self, n, 0)

    @classmethod
    def unpack_from(cls, buffer, offset=0):
        fields = cls.__struct__.unpack_from(buffer, offset)
        instance = cls()
        for n, v in zip(cls.__names__, fields):
            setattr(instance, n, v)
        return instance

    def pack(self):
        return self.__struct__.pack(*[ getattr(self, n) for n in self.__names__ ])

    def __str__(self):
        return '(' + ', '.join([ "'%s': %s" % (n, repr(getattr(self, n))) for n in self.__names__ if n is not None ]) + ')'

#
# some defs for flat DT data
#

class HeaderV17(BetterStruct):
    __endian__ = '>'
    __fields__ = [
        ('I', 'magic'),
        ('I', 'totalsize'),
        ('I', 'off_dt_struct'),
        ('I', 'off_dt_strings'),
        ('I', 'off_mem_rsvmap'),
        ('I', 'version'),
        ('I', 'last_comp_version'),
        ('I', 'boot_cpuid_phys'),
        ('I', 'size_dt_strings'),
        ('I', 'size_dt_struct'),
    ]

class RRHeader(BetterStruct):
    __endian__ = '>'
    __fields__ = [
        ('Q', 'address'),
        ('Q', 'size'),
    ]

class PropHeader(BetterStruct):
    __endian__ = '>'
    __fields__ = [
        ('I', 'value_size'),
        ('I', 'name_offset'),
    ]

OF_DT_HEADER = 0xd00dfeed
OF_DT_BEGIN_NODE = 1
OF_DT_END_NODE = 2
OF_DT_PROP = 3
OF_DT_END = 9

class StringsBlock:
    def __init__(self, values=None):
        if values is None:
            self.values = []
        else:
            self.values = values

    def __getitem__(self, at):
        if isinstance(at, str):
            offset = 0
            for value in self.values:
                if value == at:
                    break
                offset += len(value) + 1
            else:
                self.values.append(at)
            return offset

        elif isinstance(at, int):
            offset = 0
            for value in self.values:
                if offset == at:
                    return value
                offset += len(value) + 1
            raise IndexError('no string found corresponding to the given offset')

        else:
            raise TypeError('only strings and integers are accepted')

class Prop:
    def __init__(self, name=None, value=None):
        self.name = name
        self.value = value

    def clone(self):
        return Prop(self.name, self.value)

    def __repr__(self):
        return "<Prop(name='%s', value=%s>" % (self.name, repr(self.value))

class Node:
    def __init__(self, name=None):
        self.name = name
        self.props = []
        self.children = []

    def clone(self):
        o = Node(self.name)
        o.props = [ x.clone() for x in self.props ]
        o.children = [ x.clone() for x in self.children ]
        return o

    def __getitem__(self, index):
        return self.children[index]

    def __repr__(self):
        return "<Node('%s'), %s, %s>" % (self.name, repr(self.props), repr(self.children))

#
# flat DT to memory
#

def parse_strings(strings):
    strings = strings.split(b'\x00')
    return StringsBlock(strings)

def parse_struct(stream):
    tag = bytearray(stream.read(4))[3]
    if tag == OF_DT_BEGIN_NODE:
        name = b''
        while b'\x00' not in name:
            name += stream.read(4)
        name = name.rstrip(b'\x00')
        node = Node(name)

        item = parse_struct(stream)
        while item is not None:
            if isinstance(item, Node):
                node.children.append(item)
            elif isinstance(item, Prop):
                node.props.append(item)
            item = parse_struct(stream)

        return node

    elif tag == OF_DT_PROP:
        h = PropHeader.unpack_from(stream.read(PropHeader.size))
        length = (h.value_size + 3) & (~3)
        value = stream.read(length)[:h.value_size]
        prop = Prop(h.name_offset, value)
        return prop

    elif tag in (OF_DT_END_NODE, OF_DT_END):
        return None

    else:
        raise ValueError('unexpected tag value')

def read_fdt(fp):
    header = HeaderV17.unpack_from(fp.read(HeaderV17.size))
    if header.magic != OF_DT_HEADER:
        raise ValueError('invalid magic value %08x; expected %08x' % (header.magic, OF_DT_HEADER))
    # TODO: read/parse reserved regions
    fp.seek(header.off_dt_struct)
    structs = fp.read(header.size_dt_struct)
    fp.seek(header.off_dt_strings)
    strings = fp.read(header.size_dt_strings)
    strblock = parse_strings(strings)
    root = parse_struct(BytesIO(structs))

    return root, strblock

#
# memory to flat DT
#

def compose_structs_r(item):
    t = bytearray()

    if isinstance(item, Node):
        t.extend(struct.pack('>I', OF_DT_BEGIN_NODE))
        if type(item.name) == str:
            item.name = bytes(item.name, 'utf-8')
        name = item.name + b'\x00'
        if len(name) & 3:
            name += b'\x00' * (4 - (len(name) & 3))
        t.extend(name)
        for p in item.props:
            t.extend(compose_structs_r(p))
        for c in item.children:
            t.extend(compose_structs_r(c))
        t.extend(struct.pack('>I', OF_DT_END_NODE))

    elif isinstance(item, Prop):
        t.extend(struct.pack('>I', OF_DT_PROP))
        value = item.value
        h = PropHeader()
        h.name_offset = item.name
        if value:
            h.value_size = len(value)
            t.extend(h.pack())
            if len(value) & 3:
                value += b'\x00' * (4 - (len(value) & 3))
            t.extend(value)
        else:
            h.value_size = 0
            t.extend(h.pack())

    return t

def compose_structs(root):
    t = compose_structs_r(root)
    t.extend(struct.pack('>I', OF_DT_END))
    return t

def compose_strings(strblock):
    b = bytearray()
    for s in strblock.values:
        b.extend(s)
        b.append(0)
    return bytes(b)

def write_fdt(root, strblock, fp):
    header = HeaderV17()
    header.magic = OF_DT_HEADER
    header.version = 17
    header.last_comp_version = 16
    fp.write(header.pack())

    header.off_mem_rsvmap = fp.tell()
    fp.write(RRHeader().pack())

    structs = compose_structs(root)
    header.off_dt_struct = fp.tell()
    header.size_dt_struct = len(structs)
    fp.write(structs)

    strings = compose_strings(strblock)
    header.off_dt_strings = fp.tell()
    header.size_dt_strings = len(strings)
    fp.write(strings)

    header.totalsize = fp.tell()

    fp.seek(0)
    fp.write(header.pack())

#
# pretty printing / converting to DT source
#

def prety_print_value(value):
    if not value:
        return "''"
    if value[-1] == b'\x00':
        printable = True
        for x in value[:-1]:
            x = ord(x)
            if x != 0 and (x < 0x20 or x > 0x7F):
                printable = False
                break
        if printable:
            value = value[:-1]
            return ', '.join(''' + x + ''' for x in value.split(b'\x00'))
    value = binascii.hexlify(value)
    if len(value) > 0x80:
        return '[' + ' '.join(wrap(value[:0x80], 2)) + '] # truncated'
    return '[' + ' '.join(wrap(value, 2)) + ']'

def pretty_print_r(node, strblock, indent=0):
    spaces = '  ' * indent
    print((spaces + '%s {' % (node.name if node.name else '/')))
    for p in node.props:
        print((spaces + '  %s = %s;' % (strblock[p.name], prety_print_value(p.value))))
    for c in node.children:
        pretty_print_r(c, strblock, indent+1)
    print((spaces + '};'))

def pretty_print(node, strblock):
    print('/dts-v1/;')
    pretty_print_r(node,strblock, 0)

#
# manipulating the DT structure
#

def manipulate(root, strblock):
    kernel_node = root[0][0]
    fake_kernel = kernel_node.clone()
    fake_kernel.name = 'kernel-2'
    fake_kernel.children = []
    fake_kernel.props[0].value = b'Super 1337 kernel\x00'
    root[0].children.append(fake_kernel)

    root[1].props[0].value = b'conf-2\x00'
    fake_conf = root[1][0].clone()
    fake_conf.name = 'conf-2'
    fake_conf.props[0].value = b'kernel-2\x00'
    fake_conf.props[1].value = b'fdt-1\x00'
    root[1].children.append(fake_conf)

    return root, strblock

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'rb') as fp:
        root, strblock = read_fdt(fp)

    pretty_print(root, strblock)

    if True:
        root, strblock = manipulate(root, strblock)
        with open('blah', 'w+b') as fp:
            write_fdt(root, strblock, fp)
