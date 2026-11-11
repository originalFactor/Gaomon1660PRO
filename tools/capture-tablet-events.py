#!/usr/bin/env python3
"""Read only the 1060PRO's own pen and keypad events for 30 seconds."""
import os
import selectors
import struct
import sys
import time

paths = {
    'pen': '/dev/input/by-id/usb-GAOMON_GAOMON_Tablet_1060PRO-if01-event-mouse',
    'keys': '/dev/input/by-id/usb-GAOMON_GAOMON_Tablet_1060PRO-if02-event-kbd',
}
fmt = struct.Struct('@llHHi')
selector = selectors.DefaultSelector()
fds = []
try:
    for label, path in paths.items():
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
        fds.append(fd)
        selector.register(fd, selectors.EVENT_READ, label)
    print('开始采集 30 秒：依次按两个侧键，再用笔点击两个顶部软键。', flush=True)
    deadline = time.monotonic() + 30
    x = y = None
    while time.monotonic() < deadline:
        for key, _ in selector.select(max(0, deadline - time.monotonic())):
            try:
                data = os.read(key.fd, fmt.size * 64)
            except BlockingIOError:
                continue
            for offset in range(0, len(data) - fmt.size + 1, fmt.size):
                _, _, typ, code, value = fmt.unpack_from(data, offset)
                if key.data == 'pen' and typ == 3 and code in (0, 1):
                    if code == 0: x = value
                    else: y = value
                elif typ == 1 and value in (0, 1):
                    print(f'{key.data} code={code} value={value} x={x} y={y}', flush=True)
except OSError as exc:
    print(f'读取失败：{exc}', file=sys.stderr)
    sys.exit(1)
finally:
    for fd in fds: os.close(fd)
