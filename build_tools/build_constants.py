from typing import NamedTuple

class TargetOSContants(NamedTuple):
    WINDOWS: str = 'win'
    LINUX: str = 'linux'
    ANDROID: str = 'android'
    IOS: str = 'ios'
    MACOS: str = 'macos'
    WEB: str = 'web'
    NX64: str = 'nx64'
    PS4: str = 'ps4'
    PS5: str = 'ps5'
    XBONE: str = 'xbone'

TargetOS = TargetOSContants()