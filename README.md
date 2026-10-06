# VolkHIDComms
A serial communication library for KMBox-style HID devices, with automatic device detection and mouse movement.

### Currently supports:
- **Connection management**
  - COM port enumeration from the registry
  - Auto-connect that probes each port and detects the device type
  - Disconnecting and connection state queries

- **Supported devices**
  - Ferrum
  - MAKCU, including switching it to 4 Mbaud
  - Generic KMBox B+ compatible devices

- **Communication**
  - Raw byte and string writes
  - Reading responses with a timeout
  - Relative mouse movement through `km.move`

## Usage

VolkHIDComms builds as a static library. Add it as a submodule, reference `VolkHIDComms.vcxproj`, and put `external\VolkHIDComms\include` in your project's **Additional Include Directories**.

It logs through [VolkLog](https://github.com/lyk64/VolkLog), which it finds at `$(SolutionDir)external\VolkLog\include`, so the consuming solution needs VolkLog as a submodule too.

```cpp
#include <VolkHIDComms/hidcomms.hh>

int main() {
    volk::hid::Connection hid;
    hid.auto_connect();

    if (hid.is_connected())
        hid.move(10, -5);
}
```

## Contributors
- **Creator:** [lyk64](https://github.com/lyk64)
- [Stipulations](https://github.com/Stipulations)

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
