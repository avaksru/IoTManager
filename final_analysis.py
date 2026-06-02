#!/usr/bin/env python3
"""Final comprehensive firmware size analysis."""

import re
import os
import subprocess
from collections import defaultdict

def analyze_archive(size_exe, archive_path):
    """Use xtensa-size to analyze archive and return per-object sizes."""
    try:
        result = subprocess.run(
            [size_exe, archive_path],
            capture_output=True, text=True, timeout=60
        )
        lines = result.stdout.strip().split('\n')
        objects = []
        for line in lines[1:]:  # Skip header
            if line.strip():
                parts = line.split()
                if len(parts) >= 6:
                    text = int(parts[0])
                    data = int(parts[1])
                    bss = int(parts[2])
                    total = int(parts[3])
                    obj_name = parts[-1].split('(')[0].replace('.o)', '')
                    objects.append({
                        'name': obj_name,
                        'text': text,
                        'data': data,
                        'bss': bss,
                        'total': total
                    })
        return objects
    except Exception as e:
        print(f"Error: {e}")
        return []

def main():
    build_dir = r'.pio\build\esp32_4mb3f'
    size_exe = r'C:\Users\admin\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-size.exe'
    map_path = os.path.join(build_dir, 'firmware.map')
    
    # Known sizes from ELF
    total_text = 1301216
    total_data = 315112
    total_bss = 31193
    total = total_text + total_data + total_bss
    
    print("=" * 80)
    print("ANALYSIS OF FIRMWARE SIZE BY MODULES/LIBRARIES")
    print("Build: esp32_4mb3f")
    print("=" * 80)
    
    print(f"\n{'TOTAL FIRMWARE SIZE:':40}")
    print(f"  .text (code):     {total_text:>10,} bytes  ({total_text/1024:>6.1f} KB)")
    print(f"  .data:            {total_data:>10,} bytes  ({total_data/1024:>6.1f} KB)")
    print(f"  .bss:             {total_bss:>10,} bytes  ({total_bss/1024:>6.1f} KB)")
    print(f"  {'='*45}")
    print(f"  TOTAL:            {total:>10,} bytes  ({total/1024/1024:.2f} MB)")
    
    # Find all archives
    archives = {
        'NimBLE-Arduino': 'lib760/libNimBLE-Arduino.a',
        'FrameworkArduino': 'libFrameworkArduino.a',
        'WiFi': 'libd09/libWiFi.a',
        'WebServer': 'libcf6/libWebServer.a',
        'WebSockets': 'libf4d/libWebSockets.a',
        'HTTPClient': 'lib8bf/libHTTPClient.a',
        'Ethernet': 'libe9f/libEthernet.a',
        'WiFiClientSecure': 'lib446/libWiFiClientSecure.a',
        'FS': 'lib735/libFS.a',
        'HTTPUpdate': 'libc17/libHTTPUpdate.a',
        'PubSubClient': 'lib532/libPubSubClient.a',
        'Wire': 'lib51a/libWire.a',
        'AsyncUDP': 'libe23/libAsyncUDP.a',
        'EspSoftwareSerial': 'lib8a7/libEspSoftwareSerial.a',
        'DallasTemperature': 'lib33e/libDallasTemperature.a',
        'ESPmDNS': 'lib9b2/libESPmDNS.a',
        'Update': 'libfd6/libUpdate.a',
        'OneWire': 'liba8d/libOneWire.a',
        'SPI': 'lib7f0/libSPI.a',
        'SPIFFS': 'lib934/libSPIFFS.a',
        'LittleFS': 'lib275/libLittleFS.a',
        'TickerScheduler': 'lib4b3/libTickerScheduler.a',
        'Ticker': 'lib685/libTicker.a',
    }
    
    print("\n" + "=" * 80)
    print("TOP LIBRARIES BY SIZE")
    print("=" * 80)
    
    library_sizes = defaultdict(int)
    all_objects = defaultdict(list)
    
    for lib_name, archive in archives.items():
        archive_path = os.path.join(build_dir, archive)
        if not os.path.exists(archive_path):
            continue
        
        objects = analyze_archive(size_exe, archive_path)
        
        if objects:
            lib_total = sum(o['total'] for o in objects)
            library_sizes[lib_name] = lib_total
            all_objects[lib_name] = objects
    
    # Sort and display libraries
    sorted_libs = sorted(library_sizes.items(), key=lambda x: x[1], reverse=True)
    print(f"\n{'Library':30} {'Size':>12} {'%':>7}")
    print("-" * 55)
    for lib, size in sorted_libs:
        pct = (size / total) * 100
        print(f"  {lib:30s} {size:>8,} B ({size/1024:>5.1f} KB) {pct:>5.1f}%")
    
    # Top 20 largest individual object files
    print("\n" + "=" * 80)
    print("TOP 20 LARGEST OBJECT FILES (from libraries)")
    print("=" * 80)
    
    all_objs = []
    for lib, objects in all_objects.items():
        for obj in objects:
            if obj['total'] > 100:  # Only significant files
                obj['lib'] = lib
                all_objs.append(obj)
    
    all_objs.sort(key=lambda x: x['total'], reverse=True)
    print(f"\n{'#':>2} {'Module':45} {'Library':20} {'Size':>10}")
    print("-" * 85)
    for i, obj in enumerate(all_objs[:20], 1):
        pct = (obj['total'] / total) * 100
        print(f"  {i:2d}. {obj['name']:43s} {obj['lib']:20s} {obj['total']:>7,} B ({pct:>4.1f}%)")
    
    # Summary by category
    print("\n" + "=" * 80)
    print("SIZE BY CATEGORY")
    print("=" * 80)
    
    categories = {
        'WiFi + Network': ['WiFi', 'WiFiClientSecure', 'ESPmDNS', 'AsyncUDP'],
        'Web Services': ['WebServer', 'HTTPClient', 'WebSockets', 'Update', 'HTTPUpdate'],
        'Storage (FS)': ['LittleFS', 'FS', 'SPIFFS'],
        'BLE': ['NimBLE-Arduino'],
        'MQTT': ['PubSubClient'],
        'Framework': ['FrameworkArduino'],
        'Hardware Drivers': ['SPI', 'Wire', 'Ticker', 'TickerScheduler'],
        'Sensors': ['DallasTemperature', 'OneWire', 'EspSoftwareSerial'],
        'Ethernet': ['Ethernet'],
    }
    
    print(f"\n{'Category':35} {'Size':>12} {'%':>7}")
    print("-" * 60)
    for cat, libs in categories.items():
        cat_size = sum(library_sizes.get(lib, 0) for lib in libs)
        if cat_size > 0:
            pct = (cat_size / total) * 100
            print(f"  {cat:35s} {cat_size:>7,} B ({cat_size/1024:>5.1f} KB) {pct:>5.1f}%")
    
    # Remaining bytes estimate
    project_size = sum(s for lib, s in sorted_libs)
    estimated_remaining = total - project_size
    if estimated_remaining > 0:
        pct = (estimated_remaining / total) * 100
        print(f"  {'Other (project code)':35s} {estimated_remaining:>7,} B ({estimated_remaining/1024:>5.1f} KB) {pct:>5.1f}%")

if __name__ == '__main__':
    main()