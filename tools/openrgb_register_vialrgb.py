#!/usr/bin/env python3
"""Register the CH552 VialRGB device in OpenRGB's settings.

OpenRGB's VialRGB detector does not scan every HID device.  It only checks
devices listed in the "QMKVialRGBDevices" section of OpenRGB.json, plus a
couple of hard-coded VID/PIDs.  Adding the device once makes OpenRGB detect
it automatically on every later scan.

This script edits OpenRGB.json directly, so it is equivalent to adding a
"QMK VialRGB Device" in OpenRGB's GUI, without clicking through the UI.

Close OpenRGB before running it.

Examples:
    python tools/openrgb_register_vialrgb.py
    python tools/openrgb_register_vialrgb.py --path "C:\\Users\\me\\AppData\\Roaming\\OpenRGB\\OpenRGB.json"
    python tools/openrgb_register_vialrgb.py --vid 1209 --pid 5524 --name "CH552 QMK 4Key"
"""

import argparse
import json
import os
import shutil
import sys
from pathlib import Path


def default_paths():
    paths = []
    appdata = os.environ.get("APPDATA")
    if appdata:
        paths.append(Path(appdata) / "OpenRGB" / "OpenRGB.json")

    xdg = os.environ.get("XDG_CONFIG_HOME")
    if xdg:
        paths.append(Path(xdg) / "OpenRGB" / "OpenRGB.json")
    else:
        paths.append(Path.home() / ".config" / "OpenRGB" / "OpenRGB.json")

    paths.append(Path.home() / "Library" / "Application Support" / "OpenRGB" / "OpenRGB.json")
    return paths


def find_settings(explicit):
    if explicit:
        path = Path(explicit).expanduser()
        if not path.exists():
            sys.exit(f"OpenRGB settings not found: {path}")
        return path

    for path in default_paths():
        if path.exists():
            return path

    tried = "\n  ".join(str(p) for p in default_paths())
    sys.exit(
        "Could not find OpenRGB.json. Start OpenRGB once, then close it, or "
        "pass --path.\nTried:\n  " + tried
    )


def normalized_hex(value, fallback):
    if value is None:
        return fallback
    text = str(value).strip()
    if text.lower().startswith("0x"):
        text = text[2:]
    return f"{int(text, 16):04X}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--path", help="path to OpenRGB.json")
    parser.add_argument("--vid", default="1209", help="USB vendor ID (hex)")
    parser.add_argument("--pid", default="5524", help="USB product ID (hex)")
    parser.add_argument("--name", default="CH552 QMK 4Key",
                        help="name shown in OpenRGB")
    args = parser.parse_args()

    path = find_settings(args.path)
    vid = normalized_hex(args.vid, "1209")
    pid = normalized_hex(args.pid, "5524")

    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        data = {}
    except json.JSONDecodeError as e:
        sys.exit(f"{path} is not valid JSON: {e}")

    if not isinstance(data, dict):
        sys.exit(f"{path} does not contain a JSON object")

    old_entry = None
    old_devices = data.get("QMKOpenRGBDevices", {}).get("devices", [])
    if isinstance(old_devices, list):
        for entry in old_devices:
            if not isinstance(entry, dict):
                continue
            try:
                old_vid = normalized_hex(entry.get("usb_vid"), vid)
                old_pid = normalized_hex(entry.get("usb_pid"), pid)
            except (TypeError, ValueError):
                continue
            if old_vid == vid and old_pid == pid:
                old_entry = entry
                break

    if old_entry and args.name == parser.get_default("name"):
        args.name = old_entry.get("name", args.name)

    section = data.setdefault("QMKVialRGBDevices", {})
    if not isinstance(section, dict):
        sys.exit("QMKVialRGBDevices exists but is not a JSON object")

    devices = section.get("devices")
    if not isinstance(devices, list):
        devices = []
        section["devices"] = devices

    kept = []
    for entry in devices:
        if not isinstance(entry, dict):
            kept.append(entry)
            continue
        try:
            entry_vid = normalized_hex(entry.get("usb_vid"), vid)
            entry_pid = normalized_hex(entry.get("usb_pid"), pid)
        except (TypeError, ValueError):
            kept.append(entry)
            continue
        if entry_vid == vid and entry_pid == pid:
            continue
        kept.append(entry)

    kept.append({
        "name": args.name,
        "usb_vid": vid,
        "usb_pid": pid,
    })
    section["devices"] = kept

    backup = path.with_suffix(path.suffix + ".bak")
    if path.exists():
        shutil.copy2(path, backup)

    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=4)
        f.write("\n")
    os.replace(tmp, path)

    print(f"updated {path}")
    print(f"  QMKVialRGBDevices: {args.name} ({vid}:{pid})")
    if backup.exists():
        print(f"  backup: {backup}")
    print("Close OpenRGB before editing; start it again and rescan.")


if __name__ == "__main__":
    main()
