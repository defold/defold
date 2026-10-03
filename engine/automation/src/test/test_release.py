"""Reject automation/inspection implementations and capture imports in release images."""
from pathlib import Path
import re
import sys


# Only null entry points may remain; projection tables previously escaped the module exclusion check.
def verify_release(path):
    data = Path(path).read_bytes()
    forbidden = (b'automation-bridge/v3', b'automation_bridge', b'AutomationBridgeContext',
                 b'EngineAutomation', b'ScreenCaptureKit', b'MFCreateSinkWriterFromURL',
                 b'GraphicsCaptureItem', b'CreateDirect3D11DeviceFromDXGIDevice',
                 b'AccumulateInspectionProjection')
    present = [name.decode() for name in forbidden if name in data]
    null_functions = (b'CheckExtensions', b'HasPendingInput', b'BeforeInput',
                      b'AfterInput', b'ServiceTick', b'AfterUpdate')
    for match in re.finditer(rb'[\x20-\x7e]{4,}', data):
        symbol = match.group()
        if b'InspectionProjection' in symbol and b'dmHashTable' in symbol:
            present.append('inspection projection table implementation')
        # Unstripped ELF debug metadata can contain the namespace on its own.
        if b'dmAutomation' in symbol and symbol != b'dmAutomation':
            # Accept the null API's Itanium and MSVC symbol spellings.
            is_null = any(b'dmAutomation' + str(len(name)).encode() + name + b'E' in symbol or
                          b'?' + name + b'@dmAutomation@@' in symbol for name in null_functions)
            if not is_null:
                present.append(symbol.decode())
    if present:
        raise AssertionError(f'debug automation/inspection leaked into {path}: {sorted(set(present))}')


if __name__ == '__main__':
    verify_release(sys.argv[1])
