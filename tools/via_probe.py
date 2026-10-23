#!/usr/bin/env python3
"""Small VIA raw-HID probe for the CH552 QMK firmware.

Works with both the 3x4 board (1209:5523) and the 4-key board
(1209:5524).

Requires pyusb:
    pip install pyusb

Examples:
    ./via_probe.py info
    ./via_probe.py get 0 0 0
    ./via_probe.py set 0 0 0 0x0004     # layer 0, row 0, col 0 -> KC_A
    ./via_probe.py rgb
    ./via_probe.py rgb-effect 3
    ./via_probe.py rgb-brightness 96
    ./via_probe.py rgb-color 0 255
    ./via_probe.py save-rgb
"""

import argparse
import sys

try:
    import usb.core
    import usb.util
except ImportError:
    sys.exit("pyusb is required: pip install pyusb")

# Default 3x4 board and the 4-key board profile.
DEVICES = [
    (0x1209, 0x5523, "3x4"),
    (0x1209, 0x5524, "4key"),
]
RAW_OUT = 0x03
RAW_IN = 0x82
RAW_SIZE = 32


def open_device():
    dev = None

    for vid, pid, name in DEVICES:
        dev = usb.core.find(idVendor=vid, idProduct=pid)
        if dev is not None:
            break

    if dev is None:
        ids = ", ".join(f"{v:04x}:{p:04x}" for v, p, _ in DEVICES)
        sys.exit(f"device not found (tried {ids})")

    # The device is already configured by the OS; calling
    # set_configuration() here fails with EBUSY while usbhid owns the
    # keyboard interface.  Just claim the raw HID interface.
    interface = None
    for cfg in dev:
        for intf in cfg:
            if any(ep.bEndpointAddress == RAW_OUT for ep in intf):
                interface = intf
                break
        if interface is not None:
            break

    if interface is None:
        sys.exit("raw HID interface (endpoint 0x03) not found")

    number = interface.bInterfaceNumber
    try:
        if dev.is_kernel_driver_active(number):
            dev.detach_kernel_driver(number)
    except (NotImplementedError, usb.core.USBError):
        pass

    usb.util.claim_interface(dev, number)
    return dev


def xfer(dev, payload):
    data = bytes(payload) + bytes(RAW_SIZE - len(payload))
    dev.write(RAW_OUT, data, timeout=1000)
    return bytes(dev.read(RAW_IN, RAW_SIZE, timeout=1000))


def cmd_info(dev, _args):
    r = xfer(dev, [0x01])
    print(f"VIA protocol : {r[1] << 8 | r[2]:04x}")

    r = xfer(dev, [0x11])
    print(f"layers       : {r[1]}")

    r = xfer(dev, [0x02, 0x04])
    print(f"firmware     : {r[2] << 24 | r[3] << 16 | r[4] << 8 | r[5]:08x}")

    r = xfer(dev, [0x02, 0x01])
    print(f"uptime       : {r[2] << 24 | r[3] << 16 | r[4] << 8 | r[5]} ms")


def cmd_get(dev, args):
    r = xfer(dev, [0x04, args.layer, args.row, args.col])
    keycode = (r[4] << 8) | r[5]
    print(f"layer {args.layer} row {args.row} col {args.col}: 0x{keycode:04x}")


def cmd_set(dev, args):
    value = int(args.keycode, 0)
    xfer(dev, [0x05, args.layer, args.row, args.col,
               (value >> 8) & 0xFF, value & 0xFF])
    print(f"set to 0x{value:04x}")


def cmd_matrix(dev, _args):
    r = xfer(dev, [0x02, 0x03, 0])
    print("row bits:", " ".join(f"{b:02x}" for b in r[3:12]))


def rgb_get(dev):
    values = {}
    for value_id, name in [(1, "brightness"), (2, "effect"),
                           (3, "speed")]:
        r = xfer(dev, [0x08, 0x02, value_id])
        values[name] = r[3]
    r = xfer(dev, [0x08, 0x02, 4])
    values["hue"] = r[3]
    values["sat"] = r[4]
    return values


def cmd_rgb(dev, _args):
    v = rgb_get(dev)
    print(" ".join(f"{k}={v[k]}" for k in
                   ["brightness", "effect", "speed", "hue", "sat"]))


def cmd_rgb_set(dev, args):
    if args.what == "brightness":
        xfer(dev, [0x07, 0x02, 1, args.value & 0xFF])
    elif args.what == "effect":
        xfer(dev, [0x07, 0x02, 2, args.value & 0xFF])
    elif args.what == "speed":
        xfer(dev, [0x07, 0x02, 3, args.value & 0xFF])
    elif args.what == "color":
        xfer(dev, [0x07, 0x02, 4, args.value & 0xFF, args.sat & 0xFF])
    print("ok (use save-rgb to persist)")


def cmd_save_rgb(dev, _args):
    xfer(dev, [0x09, 0x02, 0])
    print("saved")


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("info")
    sub.add_parser("matrix")
    sub.add_parser("rgb")
    sub.add_parser("save-rgb")

    p = sub.add_parser("get")
    p.add_argument("layer", type=int)
    p.add_argument("row", type=int)
    p.add_argument("col", type=int)
    p.set_defaults(func=cmd_get)

    p = sub.add_parser("set")
    p.add_argument("layer", type=int)
    p.add_argument("row", type=int)
    p.add_argument("col", type=int)
    p.add_argument("keycode")
    p.set_defaults(func=cmd_set)

    p = sub.add_parser("rgb-set")
    p.add_argument("what", choices=["brightness", "effect", "speed", "color"])
    p.add_argument("value", type=lambda x: int(x, 0))
    p.add_argument("sat", nargs="?", type=lambda x: int(x, 0), default=255)
    p.set_defaults(func=cmd_rgb_set)

    args = parser.parse_args()
    dev = open_device()
    try:
        if args.command == "info":
            cmd_info(dev, args)
        elif args.command == "matrix":
            cmd_matrix(dev, args)
        elif args.command == "rgb":
            cmd_rgb(dev, args)
        elif args.command == "save-rgb":
            cmd_save_rgb(dev, args)
        else:
            args.func(dev, args)
    finally:
        usb.util.dispose_resources(dev)


if __name__ == "__main__":
    main()
