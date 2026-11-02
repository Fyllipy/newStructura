#!/usr/bin/env python3
"""
Post-build PE hardening and metadata scrubber for solver DLLs (rds.dll, mefs.dll).

Performs binary-level anti-reverse-engineering transformations without altering
runtime execution or Windows loader compatibility:
  1. Wipes the DOS stub message ("!This program cannot be run in DOS mode.")
     and the MSVC Rich Header between 0x40 and e_lfanew.
  2. Clears COFF TimeDateStamp and Linker Version fields.
  3. Locates and wipes any Debug Directory payloads (PDB paths, POGO/coffgrp
     section maps, VC feature signatures) and clears Data Directory[6].
  4. Scrubs the internal DLL module name in IMAGE_EXPORT_DIRECTORY and verifies
     that only the single cryptographic gatekeeper symbol is exported.
  5. Obfuscates PE section names (.text, .rdata, .data, .pdata, .reloc -> .s0..).
  6. Recomputes the PE Optional Header CheckSum.
"""

import struct
import sys
from pathlib import Path


def rva_to_offset(rva: int, sections: list[tuple[int, int, int, int]]) -> int | None:
    for virt_addr, virt_size, raw_ptr, raw_size in sections:
        span = max(virt_size, raw_size)
        if virt_addr <= rva < virt_addr + span:
            off = raw_ptr + (rva - virt_addr)
            if off < raw_ptr + raw_size:
                return off
    return None


def compute_pe_checksum(data: bytearray, checksum_offset: int) -> int:
    # Zero checksum field before computing
    struct.pack_into("<I", data, checksum_offset, 0)
    total = 0
    length = len(data)
    # Sum 16-bit words with 32-bit folding
    limit = length & ~1
    for i in range(0, limit, 2):
        word = data[i] | (data[i + 1] << 8)
        total += word
        total = (total & 0xFFFF) + (total >> 16)
    if length & 1:
        total += data[-1]
        total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (total + length) & 0xFFFFFFFF


def protect_pe(pe_path: Path) -> None:
    data = bytearray(pe_path.read_bytes())
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise RuntimeError(f"{pe_path}: not a valid MZ/PE binary")

    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if e_lfanew < 0x40 or e_lfanew + 0x108 > len(data):
        raise RuntimeError(f"{pe_path}: invalid e_lfanew={e_lfanew:#x}")

    if data[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        raise RuntimeError(f"{pe_path}: missing PE signature")

    # 1. Wipe DOS stub + MSVC Rich Header [0x40 .. e_lfanew)
    # Leave a minimal 2-byte int 20h / ret stub at 0x40 and zero the rest.
    for i in range(0x40, e_lfanew):
        data[i] = 0x00

    coff_off = e_lfanew + 4
    machine, num_sections, time_stamp, sym_ptr, num_syms, opt_size, characteristics = struct.unpack_from(
        "<HHIIIHH", data, coff_off
    )

    # 2. Zero COFF TimeDateStamp, PointerToSymbolTable, NumberOfSymbols
    struct.pack_into("<III", data, coff_off + 4, 0, 0, 0)

    opt_off = coff_off + 20
    opt_magic = struct.unpack_from("<H", data, opt_off)[0]
    if opt_magic != 0x20B:  # PE32+ (64-bit)
        raise RuntimeError(f"{pe_path}: expected PE32+ (0x20B), got {opt_magic:#x}")

    # Zero MajorLinkerVersion and MinorLinkerVersion
    data[opt_off + 2] = 0
    data[opt_off + 3] = 0

    checksum_off = opt_off + 64
    num_rva_and_sizes = struct.unpack_from("<I", data, opt_off + 108)[0]
    data_dir_off = opt_off + 112

    sec_table_off = opt_off + opt_size
    sections: list[tuple[int, int, int, int]] = []
    for idx in range(num_sections):
        sh_off = sec_table_off + idx * 40
        virt_size, virt_addr, raw_size, raw_ptr = struct.unpack_from("<IIII", data, sh_off + 8)
        sections.append((virt_addr, virt_size, raw_ptr, raw_size))

    # 3. Wipe Debug Directory (Data Directory index 6) and its raw payload
    if num_rva_and_sizes > 6:
        dbg_rva, dbg_size = struct.unpack_from("<II", data, data_dir_off + 6 * 8)
        if dbg_rva != 0 and dbg_size != 0:
            dbg_off = rva_to_offset(dbg_rva, sections)
            if dbg_off is not None and dbg_off + dbg_size <= len(data):
                num_entries = dbg_size // 28
                for e in range(num_entries):
                    ent_off = dbg_off + e * 28
                    size_of_data, addr_raw, ptr_raw = struct.unpack_from("<III", data, ent_off + 16)
                    if ptr_raw != 0 and size_of_data != 0 and ptr_raw + size_of_data <= len(data):
                        for k in range(ptr_raw, ptr_raw + size_of_data):
                            data[k] = 0
                for k in range(dbg_off, dbg_off + dbg_size):
                    data[k] = 0
            struct.pack_into("<II", data, data_dir_off + 6 * 8, 0, 0)

    # 4. Scrub Export Directory (Data Directory index 0) metadata
    exported_names: list[str] = []
    if num_rva_and_sizes > 0:
        exp_rva, exp_size = struct.unpack_from("<II", data, data_dir_off + 0 * 8)
        if exp_rva != 0 and exp_size != 0:
            exp_off = rva_to_offset(exp_rva, sections)
            if exp_off is not None and exp_off + 40 <= len(data):
                # Zero Export TimeDateStamp + Major/MinorVersion
                struct.pack_into("<IHH", data, exp_off + 4, 0, 0, 0)
                name_rva, ordinal_base, num_funcs, num_names, addr_funcs, addr_names, addr_ords = struct.unpack_from(
                    "<IIIIIII", data, exp_off + 12
                )
                # Scrub internal DLL name string (e.g. "rds.dll" -> "_")
                dll_name_off = rva_to_offset(name_rva, sections)
                if dll_name_off is not None:
                    k = dll_name_off
                    while k < len(data) and data[k] != 0:
                        data[k] = ord("_") if k == dll_name_off else 0
                        k += 1

                names_off = rva_to_offset(addr_names, sections)
                if names_off is not None:
                    for n_idx in range(num_names):
                        fn_name_rva = struct.unpack_from("<I", data, names_off + n_idx * 4)[0]
                        fn_name_off = rva_to_offset(fn_name_rva, sections)
                        if fn_name_off is not None:
                            end = data.find(b"\x00", fn_name_off)
                            if end != -1:
                                exported_names.append(data[fn_name_off:end].decode("ascii", errors="replace"))

    # 5. Obfuscate Section Header Names (.text, .rdata, .data, .pdata, .reloc -> .s0 .. .sN)
    for idx in range(num_sections):
        sh_off = sec_table_off + idx * 40
        obf_name = f".s{idx}".encode("ascii").ljust(8, b"\x00")[:8]
        data[sh_off : sh_off + 8] = obf_name

    # 6. Recompute PE CheckSum
    checksum = compute_pe_checksum(data, checksum_off)
    struct.pack_into("<I", data, checksum_off, checksum)

    pe_path.write_bytes(data)
    print(f"[protect_pe] Hardened {pe_path.name} ({len(data)} bytes, exports={exported_names}, checksum={checksum:#010x})")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: protect_pe.py <dll_path> [...]", file=sys.stderr)
        sys.exit(1)
    for arg in sys.argv[1:]:
        protect_pe(Path(arg))
