#!/usr/bin/env python3
"""Bounded prc-tools COFF -> NioLib initialization resource and static audit.

Palm .reloc records are (type:u16, source-section:u16, offset:u32,
target-section:u16, reserved:u16), with zero-based section indices. See
prc-tools-2.3/tools/binres.cpp, make_rloc_and_chains, in prc-tools-remix.
Only data-resident absolute 32-bit references to data/BSS or code are accepted.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path


def coff_sections(blob):
    if len(blob) < 20:
        raise ValueError("truncated COFF header")
    magic, count, _, _, _, optional, _ = struct.unpack_from(">HHIIIHH", blob)
    if magic != 0x150 or count > 16:
        raise ValueError("expected m68k COFF")
    result = []
    for i in range(count):
        off = 20 + optional + 40 * i
        if off + 40 > len(blob):
            raise ValueError("truncated section header")
        name, _, address, size, pos, _, _, _, _, _ = struct.unpack_from(">8sIIIIIIHHI", blob, off)
        name = name.rstrip(b"\0").decode("ascii")
        if name not in (".text", ".data", ".bss", ".reloc"):
            raise ValueError("unexpected section " + name)
        if name != ".bss" and (pos > len(blob) or size > len(blob) - pos):
            raise ValueError("truncated section " + name)
        result.append(dict(name=name, address=address, size=size,
                           data=blob[pos:pos + size] if name != ".bss" else b""))
    if len({s['name'] for s in result}) != len(result):
        raise ValueError("duplicate sections")
    return result


def make_image(blob):
    sections = coff_sections(blob)
    by_name = {s['name']: s for s in sections}
    text, data, bss = (by_name[n] for n in ('.text', '.data', '.bss'))
    if text['address'] or data['address'] or bss['address'] != data['size']:
        raise ValueError("unsupported section bases/gap")
    total = bss['address'] + bss['size']
    if not 0 < data['size'] <= total <= 32767 or text['size'] > 32767:
        raise ValueError("global/code size limit")
    reloc = by_name.get('.reloc', {}).get('data', b'')
    if len(reloc) % 12:
        raise ValueError("truncated Palm relocation")
    fixups = []
    for pos in range(0, len(reloc), 12):
        kind, source, offset, target, reserved = struct.unpack_from('>HHIHH', reloc, pos)
        if kind != 1 or reserved or source >= len(sections) or target >= len(sections):
            raise ValueError("unsupported Palm relocation")
        if sections[source]['name'] != '.data':
            raise ValueError("relocation outside data (code must be PC-relative)")
        target_name = sections[target]['name']
        if target_name not in ('.data', '.bss', '.text'):
            raise ValueError("invalid relocation target")
        if offset & 1 or offset + 4 > data['size']:
            raise ValueError("invalid relocation offset")
        value, = struct.unpack_from('>I', data['data'], offset)
        if value >= (text['size'] if target_name == '.text' else total):
            raise ValueError("relocation target out of range")
        if target_name == '.text' and value & 1:
            raise ValueError("unaligned code target")
        fixups.append((offset, 2 if target_name == '.text' else 1))
    fixups.sort()
    if any(b[0] < a[0] + 4 for a, b in zip(fixups, fixups[1:])):
        raise ValueError("overlapping relocations")
    image = struct.pack('>4sIII', b'NLG1', data['size'], total, len(fixups))
    image += data['data'] + b''.join(struct.pack('>II', *f) for f in fixups)
    return image, dict(code_bytes=text['size'], data_bytes=data['size'],
                       bss_bytes=bss['size'], global_bytes=total, fixups=fixups)


def prc(blob):
    if len(blob) < 78:
        raise ValueError("truncated PRC")
    attributes, = struct.unpack_from('>H', blob, 32)
    if not attributes & 1:
        raise ValueError("not a resource database")
    count, = struct.unpack_from('>H', blob, 76)
    if 78 + count * 10 + 2 > len(blob):
        raise ValueError("truncated directory")
    entries = [struct.unpack_from('>4sHI', blob, 78 + i * 10) for i in range(count)]
    resources = {}
    previous = 78 + count * 10 + 2
    for i, (kind, rid, off) in enumerate(entries):
        end = entries[i + 1][2] if i + 1 < count else len(blob)
        if off < previous or end < off or end > len(blob) or (kind, rid) in resources:
            raise ValueError("invalid resource directory")
        resources[kind, rid] = blob[off:end]
        previous = end
    return dict(name=blob[:32].split(b'\0')[0].decode(),
                type=blob[60:64].decode(), creator=blob[64:68].decode()), resources


def audit(out, objdump='m68k-palmos-objdump', nm='m68k-palmos-nm'):
    linked = out / 'NioLib'
    image, report = make_image(linked.read_bytes())
    metadata, resources = prc((out / 'NioLib.prc').read_bytes())
    app_meta, app_resources = prc((out / 'NioLibTest.prc').read_bytes())
    if metadata != dict(name='NioLib', type='libr', creator='NiL1'):
        raise ValueError("incorrect library identity")
    if app_meta != dict(name='NioLibTest', type='appl', creator='NiT1'):
        raise ValueError("incorrect application identity")
    code = next(s['data'] for s in coff_sections(linked.read_bytes()) if s['name'] == '.text')
    if resources != {(b'libr', 0): code, (b'nDat', 0): image}:
        raise ValueError("library resources differ from reviewed linked image")
    if (b'code', 0) not in app_resources or (b'code', 1) not in app_resources:
        raise ValueError("missing application startup/code")
    symbols = subprocess.check_output([nm, '-n', str(linked)], text=True)
    if re.search(r'^\s+U\s', symbols, re.M):
        raise ValueError("unresolved runtime helper")
    sym = {m[2]: int(m[0], 16) for m in re.findall(r'^([0-9a-fA-F]+)\s+(\S)\s+(\S+)$', symbols, re.M)}
    table = sym['NioLibDispatch']
    names = ['NioLibName', 'NioLibOpen', 'NioLibClose', 'NioLibSleep', 'NioLibWake',
             'NioLibGetVersion', 'NioLibGetCapabilities', 'NioLibTestGlobals']
    if 'NioLibBeginClient' in sym:
        names += ['NioLibBeginClient', 'NioLibEndClient', 'NioLibProbe', 'NioLibNetOpen',
                  'NioLibNetInfo', 'NioLibNetRead', 'NioLibNetWrite', 'NioLibNetClose',
                  'NioLibNetReady', 'NioLibClockGet', 'NioLibClockTimezone']
    if "NioLibGetConfig" in sym:
        names += ["NioLibGetConfig", "NioLibSetConfig", "NioLibGetTransportStatus"]
    if "NioLibNetJsonQuery" in sym:
        names += ["NioLibNetJsonQuery"]
    for i, name in enumerate(names):
        offset, = struct.unpack_from('>h', code, table + 2 * i)
        if table + offset != sym[name]:
            raise ValueError("incorrect dispatch ordinal " + name)
    if sym['start'] != 0 or code[:2] != b'\x60\x00':
        raise ValueError("missing explicit entry trampoline")
    pointer_targets = {sym['nl_data_pointer']: (1, sym['nl_initialized']),
                       sym['nl_function_pointer']: (2, sym['NioLibTransform'])}
    if any(dict(report['fixups']).get(off) != v[0] for off, v in pointer_targets.items()):
        raise ValueError("expected data/code pointer relocations missing")
    for off, (_, target) in pointer_targets.items():
        if struct.unpack_from('>I', image, 16 + off)[0] != target:
            raise ValueError("incorrect initialized pointer addend")
    dis = subprocess.check_output([objdump, '-d', str(linked)], text=True)
    (out / 'NioLib.dis').write_text(dis)
    (out / 'NioLib.symbols').write_text(symbols)
    for flag, suffix in (([objdump, '-d'], '.dis'), ([nm, '-n'], '.symbols')):
        (out / ('NioLibTest' + suffix)).write_text(
            subprocess.check_output(flag + [str(out / 'NioLibTest')], text=True))
    if '%a5' in dis or re.search(r'<(?:PilotMain|_GccStartup|__do_global_ctors)', dis):
        raise ValueError("caller A5 dependency or application CRT in library")
    helpers = ['globals_open', 'globals_close', 'globals_test']
    helpers += [name + '_a4' for name in names[8:]]
    for name in helpers:
        body = dis.split('<' + name + '>:', 1)[1].split('\n\n', 1)[0]
        saved = '%a4,%sp@-' in body or re.search(r'moveml[^\n]*%a[0-4]-%a[4-6],%sp@-', body)
        restored = ('%sp@+,%a4' in body or
                    re.search(r'moveml[^\n]*%fp@[^\n]*(?:%a4\b|%a[0-3]-%a[4-6])', body) or
                    re.search(r'moveal\s+%fp@\(-[0-9]+\),%a4', body))
        if not saved or not restored or 'a0b9' not in body or '%a0@(4),%a4' not in body:
            raise ValueError("missing compiler A4 entry/exit sequence " + name)
    if 'NioLibBeginClient' in sym:
        app_symbols = (out / 'NioLibTest.symbols').read_text()
        if any(name in app_symbols for name in ('_fn_req_buf', '_fn_resp_buf',
                'fn_transport_exchange', 'fn_parse_response_header', 'fn_palmos_exchange', 'fn_build_header')):
            raise ValueError("NIO core/transport duplicated in application")
        if not all(name in sym for name in ('fn_open', 'fn_open_translated', 'fn_read',
                    'fn_write', 'fn_ready', 'fn_clock_get', 'fn_clock_get_timezone', 'nl_original_exchange')):
            raise ValueError("required real NIO core/backend missing")
        if not all(re.search(r'\b' + name + r'$', app_symbols, re.M) for name in
                   ('fn_init', 'fn_open', 'fn_read', 'fn_shutdown')):
            raise ValueError("test application is not using compatibility archive")
        archive_symbols = subprocess.check_output([nm, str(out / 'libniolib-client.a')], text=True)
        (out / 'client.symbols').write_text(archive_symbols)
        report['client_archive_bytes'] = (out / 'libniolib-client.a').stat().st_size
        report['application_core_duplicated'] = False
        report['a4_helpers_verified'] = helpers
    artifacts = [out / 'NioLib.prc', out / 'NioLibTest.prc']
    if 'NioLibBeginClient' in sym:
        artifacts.append(out / 'libniolib-client.a')
    report.update(library=metadata, application=app_meta,
                  dispatch={name: hex(0xa800 + i) for i, name in enumerate(names)},
                  hardware='not tested',
                  sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in artifacts})
    (out / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    (out / 'SHA256SUMS').write_text(''.join(
        digest + '  ' + name + '\n' for name, digest in report['sha256'].items()))
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['image', 'audit'])
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    if args.mode == 'image':
        image, _ = make_image((args.directory / 'NioLib').read_bytes())
        (args.directory / 'nDat0000.bin').write_bytes(image)
    else:
        audit(args.directory)


if __name__ == '__main__':
    main()
