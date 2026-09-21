#!/usr/bin/env python3
import json, os, sys

def main():
    # Prefer S3 build; include ESP32 if present
    parts_s3 = []
    parts_esp = []
    dist = "dist"
    os.makedirs(dist, exist_ok=True)

    def add(parts, prefix, offset_app=0x10000):
        bl = f"{prefix}bootloader.bin"
        pt = f"{prefix}partition-table.bin"
        app = f"{prefix}app.bin"
        for p in (bl, pt, app):
            if not os.path.isfile(os.path.join(dist, p)):
                return False
        parts.extend([
            {"path": bl, "offset": 0},
            {"path": pt, "offset": 0x8000},
            {"path": app, "offset": offset_app},
        ])
        return True

    builds = []
    if add(parts_s3, "s3_"):
        builds.append({"chipFamily": "ESP32-S3", "parts": parts_s3})
    if add(parts_esp, "esp32_"):
        builds.append({"chipFamily": "ESP32", "parts": parts_esp})

    if not builds:
        print("no builds found in dist/", file=sys.stderr)
        return 1

    manifest = {
        "name": "M5 HW ID",
        "version": "1.0.0",
        "new_install_prompt_erase": True,
        "builds": builds,
    }
    path = os.path.join(dist, "manifest.json")
    with open(path, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    json.load(open(path))
    print(open(path).read())
    return 0

if __name__ == "__main__":
    sys.exit(main())
