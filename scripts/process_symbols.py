#!/usr/bin/env python3

import sys
import os
import subprocess

def read_symbols(symbols_file):
    """Read symbol mappings from symbols.def file.
    
    Supports two formats:
    1. Simple name (e.g., 'printf') - automatically prefixed with 'ub_'
    2. Explicit mapping (e.g., 'printf=ub_printf')
    """
    symbols = []
    if not os.path.exists(symbols_file):
        return symbols
    
    with open(symbols_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            if '=' in line:
                # Explicit mapping: original=new
                orig, new = line.split('=', 1)
                orig = orig.strip()
                new = new.strip()
                if orig and new:
                    symbols.append((orig, new))
            else:
                # Simple name: automatically add ub_ prefix
                orig = line.strip()
                if orig:
                    symbols.append((orig, f'ub_{orig}'))
    
    return symbols

def apply_symbols(obj_files, symbols):
    """Apply symbol redefinitions to object files."""
    if not symbols:
        return
    
    # Build objcopy command arguments
    redefine_args = []
    for orig, new in symbols:
        redefine_args.extend(['--redefine-sym', f'{orig}={new}'])
    
    # Apply to each object file
    for obj_file in obj_files:
        if os.path.exists(obj_file):
            cmd = ['objcopy'] + redefine_args + [obj_file]
            try:
                subprocess.run(cmd, check=False, capture_output=True)
            except:
                pass  # Ignore errors

def main():
    if len(sys.argv) < 3:
        print("Usage: process_symbols.py <symbols.def> <obj_file1> [obj_file2 ...]")
        sys.exit(1)
    
    symbols_file = sys.argv[1]
    obj_files = sys.argv[2:]
    
    symbols = read_symbols(symbols_file)
    apply_symbols(obj_files, symbols)

if __name__ == '__main__':
    main()