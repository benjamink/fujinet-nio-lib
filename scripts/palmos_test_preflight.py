"""Required Palm execution tests must never pass through missing prerequisites."""
import os
import json
from pathlib import Path
import sys

try:
    import unicorn
    if unicorn.__version__ != '2.1.4':
        raise ImportError('expected unicorn==2.1.4')
except ImportError as error:
    sys.exit('Palm tests require tests/requirements-palmos-shared.txt: ' + str(error))

out = Path(os.environ.get('PALMOS_SHARED_BUILD', 'build/palmos-shared-3.1'))
required = ['NioLib', 'NioLibTest', 'NioLib.prc', 'NioLibTest.prc',
            'libniolib-client.a', 'nDat0000.bin', 'verification.json',
            'NioLib.symbols', 'NioLibTest.symbols', 'usb.o', 'serial.o', 'json.o']
missing = [str(out / name) for name in required if not (out / name).is_file()]
if missing:
    sys.exit('Build make palmos-shared first; missing artifacts: ' + ', '.join(missing))
try:
    dispatch = json.loads((out / 'verification.json').read_text())['dispatch']
    for name in ('NioLibBeginClient', 'NioLibGetConfig', 'NioLibNetJsonQuery', 'NioLibResetTransport'):
        if name not in dispatch:
            raise ValueError('missing ' + name)
except (ValueError, KeyError) as error:
    sys.exit('Rebuild current Palm artifacts; invalid/incomplete audit: ' + str(error))
