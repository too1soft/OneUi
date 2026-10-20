"""Small, dependency-free PE reader for the release compatibility gate.

Reads normal AND delay imports, named/ordinal exports and forwarders. It never
loads the inspected binary. Malformed/unknown structures fail closed.
"""
import hashlib
import struct
from pathlib import Path


class PEError(ValueError):
    pass


class PE:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if self.data[:2] != b"MZ":
            raise PEError("missing DOS signature")
        pe = self.number(0x3C, "I")
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise PEError("missing PE signature")
        machine, count = self.unpack(pe + 4, "HH")
        if machine not in (0x14C, 0x8664):
            raise PEError(f"unsupported machine 0x{machine:x}")
        self.arch = {0x14C: "x86", 0x8664: "x64"}[machine]
        optional_size = self.number(pe + 20, "H")
        opt = pe + 24
        magic = self.number(opt, "H")
        if (magic, machine) not in ((0x10B, 0x14C), (0x20B, 0x8664)):
            raise PEError("machine and optional header disagree")
        self.width = 4 if magic == 0x10B else 8
        self.base = self.number(opt + (28 if self.width == 4 else 24), "I" if self.width == 4 else "Q")
        self.subsystem_version = self.unpack(opt + 48, "HH")
        self.headers_size = self.number(opt + 60, "I")
        directory_offset = 96 if self.width == 4 else 112
        directory_count = self.number(opt + directory_offset - 4, "I")
        if directory_count > 16 or directory_offset + directory_count * 8 > optional_size:
            raise PEError("invalid data directory table")
        self.directories = [self.unpack(opt + directory_offset + i * 8, "II") for i in range(directory_count)]
        self.sections = []
        for i in range(count):
            offset = opt + optional_size + i * 40
            virtual_size, rva, raw_size, raw = self.unpack(offset + 8, "IIII")
            self.sections.append((rva, raw_size, raw))

    def unpack(self, offset, fmt):
        size = struct.calcsize("<" + fmt)
        if offset < 0 or offset + size > len(self.data):
            raise PEError("truncated PE structure")
        return struct.unpack_from("<" + fmt, self.data, offset)

    def number(self, offset, fmt):
        return self.unpack(offset, fmt)[0]

    def rva(self, address, size=1):
        if 0 < address < self.headers_size and address + size <= min(self.headers_size, len(self.data)):
            return address
        for start, raw_size, raw in self.sections:
            if start <= address and address + size <= start + raw_size:
                offset = raw + address - start
                if offset + size <= len(self.data):
                    return offset
        raise PEError(f"unmapped RVA 0x{address:x}")

    def string(self, address):
        offset = self.rva(address)
        end = self.data.find(b"\0", offset, min(offset + 4096, len(self.data)))
        if end < 0:
            raise PEError("unterminated PE string")
        self.rva(address, end - offset + 1)
        try:
            return self.data[offset:end].decode("ascii")
        except UnicodeDecodeError as exc:
            raise PEError("non-ASCII PE import/export name") from exc

    def directory(self, index):
        return self.directories[index] if index < len(self.directories) else (0, 0)

    def thunks(self, address, va=False):
        if not address:
            raise PEError("missing import name table")
        ordinal_mask = 1 << (self.width * 8 - 1)
        for i in range(65536):
            value = self.number(self.rva(address + i * self.width, self.width), "I" if self.width == 4 else "Q")
            if value == 0:
                return
            if value & ordinal_mask:
                yield "#" + str(value & 0xFFFF)
            else:
                yield self.string(value - (self.base if va else 0) + 2)
        raise PEError("unterminated import table")

    def imports(self):
        for index, size, kind in ((1, 20, "normal"), (13, 32, "delay")):
            start, length = self.directory(index)
            if not start and not length:
                continue
            if not start or length < size:
                raise PEError(f"invalid {kind} import directory")
            terminated = False
            for delta in range(0, length - size + 1, size):
                descriptor = self.unpack(self.rva(start + delta, size), "I" * (size // 4))
                if not any(descriptor):
                    terminated = True
                    break
                if kind == "normal":
                    lookup, _, _, name, iat = descriptor
                    va = False
                else:
                    attributes, name, _, iat, lookup, _, _, _ = descriptor
                    if attributes not in (0, 1):
                        raise PEError("unknown delay import attributes")
                    va = attributes == 0
                    if va:
                        name -= self.base
                        lookup = lookup - self.base if lookup else 0
                        iat = iat - self.base if iat else 0
                module = self.string(name).lower()
                if not module or any(c in module for c in "\\/:"):
                    raise PEError("invalid imported module name")
                for symbol in self.thunks(lookup or iat, va):
                    yield {"module": module, "symbol": symbol, "kind": kind}
            if not terminated:
                raise PEError(f"unterminated {kind} import descriptors")

    def exports(self):
        start, length = self.directory(0)
        if not start:
            return {}
        _, _, _, _, _, base, count, name_count, functions, names, ordinals = self.unpack(self.rva(start, 40), "IIHHIIIIIII")
        if count > 1000000 or name_count > 1000000:
            raise PEError("invalid export counts")
        result = {}
        for i in range(count):
            address = self.number(self.rva(functions + i * 4, 4), "I")
            if address:
                result["#" + str(base + i)] = self.string(address) if start <= address < start + length else None
        for i in range(name_count):
            address = self.number(self.rva(names + i * 4, 4), "I")
            ordinal = self.number(self.rva(ordinals + i * 2, 2), "H")
            key = "#" + str(base + ordinal)
            if ordinal >= count or key not in result:
                raise PEError("invalid named export ordinal")
            result[self.string(address)] = result[key]
        return result

    def sha256(self):
        return hashlib.sha256(self.data).hexdigest()
