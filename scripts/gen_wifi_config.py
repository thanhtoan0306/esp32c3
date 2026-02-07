#!/usr/bin/env python3
"""Generate wifi_config.h from .env file."""
import os
import re

def load_env(filepath=".env"):
    env = {}
    if not os.path.exists(filepath):
        return env
    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                m = re.match(r"(\w+)=(.*)$", line)
                if m:
                    key, val = m.groups()
                    env[key] = val.strip().strip('"').strip("'")
    return env

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    env_path = os.path.join(root, ".env")
    out_path = os.path.join(root, "main", "wifi_config.h")

    env = load_env(env_path)
    ssid = env.get("WIFI_SSID", "YOUR_SSID")
    password = env.get("WIFI_PASSWORD", "YOUR_PASSWORD")

    content = f"""/**
 * Auto-generated from .env - DO NOT EDIT
 * Thay đổi trong .env rồi build lại
 */
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#define WIFI_SSID     "{ssid}"
#define WIFI_PASSWORD "{password}"

#endif
"""
    with open(out_path, "w") as f:
        f.write(content)
    print(f"Generated {out_path} from .env")

if __name__ == "__main__":
    main()
