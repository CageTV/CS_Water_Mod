"""Read-only: original water type (XCWT) of every CELL, and EDID of every WATR, from the official masters."""
import struct, zlib, json, sys, collections

DATA = 'E:/Tabula Rasa/Stock Game/Data/'
MASTERS = ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm']


def subrecords(buf):
    i = 0
    big = None
    while i + 6 <= len(buf):
        t = buf[i:i + 4].decode('latin1')
        sz = struct.unpack_from('<H', buf, i + 4)[0]
        i += 6
        if t == 'XXXX':
            big = struct.unpack_from('<I', buf, i)[0]
            i += 4
            continue
        if big is not None:
            sz, big = big, None
        yield t, buf[i:i + sz]
        i += sz


def walk(data, off, end, cb):
    while off < end:
        t = data[off:off + 4]
        size = struct.unpack_from('<I', data, off + 4)[0]
        if t == b'GRUP':
            walk(data, off + 24, off + size, cb)
            off += size
        else:
            flags = struct.unpack_from('<I', data, off + 8)[0]
            fid = struct.unpack_from('<I', data, off + 12)[0]
            body = data[off + 24:off + 24 + size]
            if flags & 0x00040000:
                body = zlib.decompress(body[4:])
            cb(t.decode('latin1'), fid, flags, body)
            off += 24 + size


def read_plugin(name):
    data = open(DATA + name, 'rb').read()
    masters = []
    res = {'cells': {}, 'watr': {}, 'wrld': {}}
    state = {'self': name}

    def norm(raw):
        idx = raw >> 24
        local = raw & 0xFFFFFF
        plug = masters[idx] if idx < len(masters) else name
        return '%06X:%s' % (local, plug)

    def cb(t, fid, flags, body):
        if t == 'TES4':
            for st, sb in subrecords(body):
                if st == 'MAST':
                    masters.append(sb.split(b'\0')[0].decode('latin1'))
            return
        if t == 'CELL':
            xcwt = None
            for st, sb in subrecords(body):
                if st == 'XCWT':
                    xcwt = norm(struct.unpack('<I', sb)[0])
            res['cells'][norm(fid)] = xcwt
        elif t == 'WATR':
            for st, sb in subrecords(body):
                if st == 'EDID':
                    res['watr'][norm(fid)] = sb.split(b'\0')[0].decode('latin1')

    walk(data, 0, len(data), cb)
    return res


if __name__ == '__main__':
    cells = {}
    watr = {}
    touched = collections.defaultdict(list)
    for m in MASTERS:
        r = read_plugin(m)
        watr.update(r['watr'])
        for k, v in r['cells'].items():
            touched[k].append((m, v))
            if v is not None:
                cells[k] = v
        print(m, len(r['cells']), 'cells,', len(r['watr']), 'water types', file=sys.stderr)
    json.dump({'cells': cells, 'watr': watr}, open('C:/Temp/waterplan/vanilla_cells.json', 'w'))
    print(len(cells), 'cells with a water type in the official masters')
