#!/usr/bin/env python3
"""Analyze detailed sizes from firmware.map file."""

import re
import os
from collections import defaultdict

def main():
    build_dir = r'.pio\buld\esp32_4mb3f'
    map_path = r'.pio\build\esp32_4mb3f\firmware.map'
    
    print("=" * 80)
    print("DETAILED FIRMWARE SIZE ANALYSIS")
    print("=" * 80)
    
    # Total size known from ELF analysis
    total_text = 1301216
    total_data = 315112
    total_bss = 31193
    total = total_text + total_data + total_bss
    
    print(f"\nTotal firmware sizes from ELF:")
    print(f"  .text: {total_text:,} bytes ({total_text/1024:.1f} KB = {total_text/1024/1024:.2f} MB)")
    print(f"  .data: {total_data:,} bytes ({total_data/1024:.1f} KB)")
    print(f"  .bss:  {total_bss:,} bytes ({total_bss/1024:.1f} KB)")
    print(f"  TOTAL: {total:,} bytes ({total/1024/1024:.2f} MB)")
    
    # Read map file
    with open(map_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Section pattern
    pattern = r'^\s*\.([a-zA-Z_]+)\s+0x[0-9a-fA-F]+\s+(0x[0-9a-fA-F]+)\s+(.+?\.o)\s*$'
    
    modules = defaultdict(lambda: {'text': 0, 'data': 0, 'bss': 0, 'rodata': 0})
    
    for line in content.split('\n'):
        match = re.match(pattern, line, re.MULTILINE)
        if match:
            section = match.group(1)
            size = int(match.group(2), 16)
            filename = match.group(3)
            
            # Skip debug/info sections
            if section.startswith('debug') or section.startswith('comment') or \
               section.startswith('group') or section.startswith('xt_') or \
               section.startswith('init') or section.startswith('fini'):
                continue
            
            if section in ['text', 'data', 'bss', 'rodata']:
                modules[filename][section] += size
    
    # Also get archive member info from the beginning of map file
    # Parse the archive listing at the start of the map file
    archive_pattern = r'(lib[A-Fa-f0-9]+/lib([^\s]+)\.a)\(([^\)]+\.o)\)'
    archive_sizes = defaultdict(lambda: defaultdict(int))
    
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Check for archive member with its reference
        archive_match = re.match(archive_pattern, line)
        if archive_match:
            archive_path = archive_match.group(1)
            lib_name = archive_match.group(2)
            obj_name = archive_match.group(3)
            
            # Look ahead for section sizes
            if i + 1 < len(lines):
                next_line = lines[i + 1]
                size_match = re.match(r'\s+\.text\s+0x0+\s+(0x[0-9a-fA-F]+)', next_line)
                if size_match:
                    size = int(size_match.group(1), 16)
                    archive_sizes[lib_name][obj_name] = size
        
        i += 1
    
    # Calculate totals by library
    print("\n" + "=" * 80)
    print("SIZE BY LIBRARY (from linked object files):")
    print("=" * 80)
    
    all_libs = defaultdict(int)
    for filename, sections in modules.items():
        if '/.pio/' in filename or '/libdeps/' in filename or '/lib/' in filename:
            # Extract lib name
            parts = filename.split('/')
            for p in parts:
                if p.startswith('lib') and p != 'lib':
                    lib_name = p.replace('.a', '')
                    break
            else:
                lib_name = parts[-2] if len(parts) > 1 else 'unknown'
            
            total_size = sections['text'] + sections['data'] + sections['bss'] + sections['rodata']
            all_libs[lib_name] += total_size
    
    for lib, size in sorted(all_libs.items(), key=lambda x: x[1], reverse=True):
        pct = (size / total) * 100 if total > 0 else 0
        print(f"  {lib:35s} {size:10,} bytes ({size/1024:6.1f} KB) ({pct:5.2f}%)")
    
    # Calculate project modules
    print("\n" + "=" * 80)
    print("PROJECT MODULES SIZE (from src/):")
    print("=" * 80)
    
    project_modules = defaultdict(int)
    for filename, sections in modules.items():
        if '/src/' in filename:
            # Get relative path
            parts = filename.split('src/')[-1].replace('.o', '')
            total_size = sections['text'] + sections['data'] + sections['bss'] + sections['rodata']
            if total_size > 0:
                project_modules[parts] = total_size
    
    for project, size in sorted(project_modules.items(), key=lambda x: x[1], reverse=True):
        pct = (size / total) * 100 if total > 0 else 0
        print(f"  {project:55s} {size:10,} bytes ({size/1024:6.1f} KB) ({pct:5.2f}%)")
    
    # Create summary
    print("\n" + "=" * 80)
    print("SUMMARY: TOP 10 LARGEST ITEMS")
    print("=" * 80)
    
    all_items = {}
    
    # Add libraries
    for lib, size in all_libs.items():
        all_items[f"[LIB] {lib}"] = size
    
    # Add project modules
    for project, size in project_modules.items():
        all_items[f"[SRC] {project}"] = size
    
    # Sort and display top 10
    sorted_items = sorted(all_items.items(), key=lambda x: x[1], reverse=True)[:10]
    for i, (name, size) in enumerate(sorted_items, 1):
        pct = (size / total) * 100 if total > 0 else 0
        print(f"  {i:2d}. {name:55s} {size:10,} bytes ({size/1024:6.1f} KB) ({pct:5.2f}%)")

if __name__ == '__main__':
    main()