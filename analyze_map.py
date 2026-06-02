#!/usr/bin/env python3
"""Analyze firmware.map file to show module sizes."""

import re
import sys
from collections import defaultdict

def parse_map_file(filepath):
    """Parse .map file and extract section sizes."""
    sizes = defaultdict(int)
    
    # Pattern for lines with section info
    # Example: " .text          0x0000000000000000       0xa6 c:/users/admin/..."
    pattern = re.compile(r'^\s*\.([a-zA-Z_]+)\s+0x[0-9a-fA-F]+\s+(0x[0-9a-fA-F]+)\s+(.+?\.o)$', re.MULTILINE)
    
    modules = {}
    
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    section = match.group(1)
                    size_hex = match.group(2)
                    filename = match.group(3)
                    
                    size = int(size_hex, 16)
                    
                    # Skip debug sections
                    if section.startswith('debug') or section.startswith('comment') or section.startswith('group'):
                        continue
                    
                    # Get module key from filename
                    if filename not in modules:
                        modules[filename] = {'text': 0, 'data': 0, 'bss': 0, 'rodata': 0, 'other': 0}
                    
                    if section == 'text':
                        modules[filename]['text'] += size
                    elif section == 'data':
                        modules[filename]['data'] += size
                    elif section == 'bss':
                        modules[filename]['bss'] += size
                    elif section == 'rodata':
                        modules[filename]['rodata'] += size
                    else:
                        modules[filename]['other'] += size
    
    except Exception as e:
        print(f"Error: {e}")
        return None
    
    return modules

def format_size(size_bytes):
    """Format size in bytes to human-readable format."""
    if size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.2f} MB ({size_bytes:,} bytes)"
    elif size_bytes >= 1024:
        return f"{size_bytes / 1024:.2f} KB ({size_bytes:,} bytes)"
    else:
        return f"{size_bytes} bytes"

def get_module_category(filename):
    """Determine category from filename."""
    if '/modules/exec/' in filename:
        return 'exec'
    elif '/modules/sensors/' in filename:
        return 'sensors'
    elif '/modules/virtual/' in filename:
        return 'virtual'
    elif '/modules/display/' in filename:
        return 'display'
    elif '/classes/' in filename:
        return 'classes'
    elif '/utils/' in filename:
        return 'utils'
    elif '/network/' in filename:
        return 'network'
    elif '/src/modules/' in filename or '/modules/' in filename:
        return 'modules'
    elif '/src/' in filename:
        return 'core'
    elif '/lib/' in filename or '/.pio/libdeps/' in filename:
        return 'lib'
    else:
        return 'other'

def get_module_name(filepath):
    """Extract short module name from path."""
    if '/modules/' in filepath:
        parts = filepath.split('/modules/')
        return parts[-1].replace('.o', '')
    elif '/src/' in filepath:
        parts = filepath.split('/src/')
        return parts[-1].replace('.o', '')
    elif '/libdeps/' in filepath:
        return filepath.split('/libdeps/')[-1].replace('.o', '')
    else:
        return filepath.split('/')[-1].replace('.o', '')

def print_results(filepath):
    """Print analysis results."""
    modules = parse_map_file(filepath)
    if not modules:
        print("No modules found")
        return
    
    # Calculate totals per module
    module_totals = []
    for filename, sections in modules.items():
        total = sections['text'] + sections['data'] + sections['bss'] + sections['rodata'] + sections['other']
        category = get_module_category(filename)
        name = get_module_name(filename)
        module_totals.append({
            'filename': filename,
            'name': name,
            'total': total,
            'text': sections['text'],
            'data': sections['data'],
            'bss': sections['bss'],
            'rodata': sections['rodata'],
            'other': sections['other'],
            'category': category
        })
    
    total_firmware = sum(m['total'] for m in module_totals)
    
    print("=" * 80)
    print("ANALYSIS OF FIRMWARE SIZE BY MODULES")
    print("=" * 80)
    print()
    print(f"Total firmware size (analyzed): {format_size(total_firmware)}")
    print()
    print("=" * 80)
    
    # Group by category
    by_category = defaultdict(int)
    for m in module_totals:
        by_category[m['category']] += m['total']
    
    print("\nSIZE BY CATEGORY:")
    print("-" * 80)
    for cat, size in sorted(by_category.items(), key=lambda x: x[1], reverse=True):
        pct = (size / total_firmware) * 100 if total_firmware > 0 else 0
        print(f"  {cat:15s} {format_size(size):35s} ({pct:5.2f}%)")
    
    # Top 30 largest modules
    print("\n" + "=" * 80)
    print("\nTOP 30 LARGEST MODULES/FILES:")
    print("-" * 80)
    
    sorted_modules = sorted(module_totals, key=lambda x: x['total'], reverse=True)[:30]
    for i, m in enumerate(sorted_modules, 1):
        pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
        print(f"  {i:2d}. {m['name'][:55]:55s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    
    # Virtual modules
    print("\n" + "=" * 80)
    print("\nVIRTUAL MODULES:")
    print("-" * 80)
    virtual = [m for m in module_totals if m['category'] == 'virtual']
    virtual.sort(key=lambda x: x['total'], reverse=True)
    if virtual:
        for m in virtual:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    else:
        print("  (no virtual modules in this build)")
    
    # Sensors modules
    print("\n" + "=" * 80)
    print("\nSENSOR MODULES:")
    print("-" * 80)
    sensors = [m for m in module_totals if m['category'] == 'sensors']
    sensors.sort(key=lambda x: x['total'], reverse=True)
    if sensors:
        for m in sensors:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    else:
        print("  (no sensor modules in this build)")
    
    # Exec modules
    print("\n" + "=" * 80)
    print("\nEXEC MODULES:")
    print("-" * 80)
    execs = [m for m in module_totals if m['category'] == 'exec']
    execs.sort(key=lambda x: x['total'], reverse=True)
    if execs:
        for m in execs:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    else:
        print("  (no exec modules in this build)")
    
    # Libraries
    print("\n" + "=" * 80)
    print("\nEXTERNAL LIBRARIES (top by size):")
    print("-" * 80)
    libs = [m for m in module_totals if m['category'] == 'lib']
    libs.sort(key=lambda x: x['total'], reverse=True)
    if libs:
        for m in libs[:20]:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    else:
        print("  (analyzing .a archives only)")
    
    # Core modules
    print("\n" + "=" * 80)
    print("\nCORE MODULES (src/):")
    print("-" * 80)
    core = [m for m in module_totals if m['category'] == 'core']
    core.sort(key=lambda x: x['total'], reverse=True)
    if core:
        for m in core:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")
    
    # Classes
    print("\n" + "=" * 80)
    print("\nCLASSES (src/classes/):")
    print("-" * 80)
    classes = [m for m in module_totals if m['category'] == 'classes']
    classes.sort(key=lambda x: x['total'], reverse=True)
    if classes:
        for m in classes:
            pct = (m['total'] / total_firmware) * 100 if total_firmware > 0 else 0
            print(f"  {m['name']:45s} {format_size(m['total']):35s} ({pct:5.2f}%)")

if __name__ == '__main__':
    filepath = '.pio\\build\\esp32_4mb3f\\firmware.map'
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    
    print(f"Analyzing: {filepath}")
    print_results(filepath)