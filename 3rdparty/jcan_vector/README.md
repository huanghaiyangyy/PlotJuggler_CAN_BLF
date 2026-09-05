# jcan_vector

Linux-only static library extracted from [jcan](https://github.com/ImArjunJ/jcan) for PlotJuggler `DataStreamVectorJcan`.

## Dependencies
- `libusb-1.0` (pkg-config)
- C++23 compiler for the implementation unit (`vector_device.cpp` / `hardware_vector.hpp`)

## udev (optional)
```
SUBSYSTEM=="usb", ATTR{idVendor}=="1248", MODE="0666"
```

## Demo
`jcan_vector_list` enumerates VN16xx channels and tries a short open/recv when a device is present.
