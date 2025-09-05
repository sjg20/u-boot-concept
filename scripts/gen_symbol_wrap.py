#!/usr/bin/env python3

import sys
import os

def read_symbols(symbols_file):
    """Read symbol mappings from symbols.def file and generate linker wrap options."""
    wrap_flags = []
    
    if not os.path.exists(symbols_file):
        return ""
    
    with open(symbols_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Parse original=new format
            if '=' in line:
                orig, new = line.split('=', 1)
                orig = orig.strip()
                new = new.strip()
                if orig and new:
                    # Use --wrap to redirect symbols at link time
                    # This wraps 'orig' so calls go to '__wrap_orig'
                    # and the original becomes '__real_orig'
                    wrap_flags.append(f'-Wl,--wrap={orig}')
                    # Also define the wrapped symbol to point to the new name
                    wrap_flags.append(f'-Wl,--defsym=__wrap_{orig}={new}')
    
    return ' '.join(wrap_flags)

def main():
    if len(sys.argv) != 2:
        print("Usage: gen_symbol_wrap.py <symbols.def>", file=sys.stderr)
        sys.exit(1)
    
    symbols_file = sys.argv[1]
    flags = read_symbols(symbols_file)
    print(flags)

if __name__ == '__main__':
    main()