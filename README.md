# L2CAP Wiimote Test
Uses the deprecated Bluez API to connect to a Wiimote without permanent pairing. This should allow some 3rd party Wii Remotes to be used.

## Dependencies
- [bluez](https://github.com/bluez/bluez)

## Build
```
cmake -S . -B build
cd build
cmake --build .
```
