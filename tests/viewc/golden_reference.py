"""Independent Basic.one byte specification; never imports the production registry.

The checked-in .hex is authored from these explicit v1 records. Updating it is a
reviewed format change, not an automatic response to a compiler test failure.
"""
import hashlib
import struct


def words(*values):
    return struct.pack('<' + 'I' * len(values), *values)


def text(value):
    data = value.encode('utf-8')
    return words(len(data)) + data


def basic_image():
    no = 0xffffffff
    source = '<template view-model="Basic"><Text :text="title"/></template>'
    node_start = source.index('<Text') + 1
    node_end = source.index('/>') + 3
    attr_start = source.index(':text') + 1
    attr_end = source.index('"/>') + 2
    strings = ['', 'Basic', 'Basic.one', 'Basic.one:0']
    blob = ''.join(strings).encode()
    offsets = [0]
    for value in strings:
        offsets.append(offsets[-1] + len(value.encode()))
    schema = b'OVSCHM1\0' + words(1, 1)
    schema += words(1) + text('Basic') + words(0, 1, 0)
    schema += words(1) + text('title') + words(4, 1, 0)
    props = [
        (1, 'basis', 3, 1), (2, 'class', 4, 1), (3, 'description', 4, 3),
        (4, 'disabled', 1, 3), (5, 'grow', 3, 1), (6, 'max', 3, 1),
        (7, 'min', 3, 1), (8, 'name', 4, 3), (9, 'shrink', 3, 1),
        (10, 'text', 4, 3), (11, 'visible', 1, 3),
    ]
    component = b'OVCOMP1\0' + words(1) + text('Text') + words(1, 11, 0)
    for ident, name, tag, flags in props:
        component += words(ident) + text(name) + words(tag, flags) + text('')
    component += text('leaf') + words(0)
    sections = {i: b'' for i in range(1, 19)}
    sections[1] = struct.pack('<5Q', *offsets) + blob
    sections[2] = schema
    sections[3] = component
    sections[4] = words(1, 0, 1, 1, 0, 0, 0, 0)
    sections[5] = words(1, 0, no, 0, 0, 0, 0, 0, 1, no, 1, 3)
    sections[9] = words(0, 10, 1, 1 << 16, 1, 0, 2, 0)
    sections[17] = (words(2, 1, node_start, 1, node_end, no) * 2
                    + words(2, 1, attr_start, 1, attr_end, no))
    stride = [0, 0, 0, 0, 32, 48, 8, 24, 16, 32, 32, 16, 24, 32, 24, 32, 24, 24, 16]
    counts = {i: len(sections[i]) // stride[i] for i in range(4, 19)}
    counts.update({1: 4, 2: 1, 3: 1})
    data = bytearray(128 + 18 * 32)
    data[:8] = b'ONEVIEW\0'
    struct.pack_into('<HHIQQIIII', data, 8, 1, 0, 128, 0, 128, 18, 0, 1, 0)
    data[48:80] = hashlib.sha256(schema).digest()
    data[80:112] = hashlib.sha256(component).digest()
    struct.pack_into('<IIQ', data, 112, 1, 1, 0)
    for kind in range(1, 19):
        data += b'\0' * (-len(data) % 8)
        struct.pack_into('<IIQQII', data, 128 + (kind - 1) * 32, kind,
                         int(kind != 17), len(data), len(sections[kind]), counts[kind], stride[kind])
        data += sections[kind]
    struct.pack_into('<Q', data, 16, len(data))
    return bytes(data)
