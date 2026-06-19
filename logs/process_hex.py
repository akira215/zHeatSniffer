#!/usr/bin/env python3
"""
Script to extract hex pairs from raw log, process with line separation,
and create spaced representation.

Configuration: Edit INPUT_FILE to process different log files.
Output files will automatically reflect the input filename.
"""

import re
import os

# ============================================================================
# CONFIGURATION
# ============================================================================
INPUT_FILE = '10ms.txt'  # Change this to process different input files
# ============================================================================


def process_log_file(input_file, output_file):
    """
    Extract hex pairs from input file and process directly,
    creating newlines each time '3a' (colon) is encountered.
    Each line starts with '3a'.
    Ignores numbers from "Read xx bytes:" entries.
    """
    try:
        # Read input file
        with open(input_file, 'r') as f:
            content = f.read()
        
        # Remove "Read X bytes:" entries to avoid treating the byte count as hex pairs
        content = re.sub(r'Read\s+\d+\s+bytes:', '', content)
        
        # Extract all hexadecimal pairs
        hex_pairs = re.findall(r'\b[0-9a-fA-F]{2}\b', content)
        
        if not hex_pairs:
            print(f"✗ No hex pairs found in {input_file}")
            return [], 0
        
        # Build output with newlines at 3a
        output_lines = []
        current_line = []
        
        for hex_code in hex_pairs:
            if hex_code.upper() == '3A':  # 3a represents colon
                # If current line has content, save it
                if current_line:
                    output_lines.append(current_line)
                # Start new line with 3a
                current_line = ['3A']
            else:
                current_line.append(hex_code)
        
        # Add any remaining hex codes
        if current_line:
            output_lines.append(current_line)
        
        # Write hex output file
        with open(output_file, 'w') as f:
            for line in output_lines:
                f.write(' '.join(line) + '\n')
        
        return output_lines, len(hex_pairs)
        
    except FileNotFoundError:
        print(f"✗ File not found: {input_file}")
        return [], 0
    except Exception as e:
        print(f"✗ Error: {e}")
        return [], 0


def create_spaced_output(hex_lines, output_file):
    """
    Create spaced representation of each line with paired hex values.
    Format: : XX XX XX ... [0D][0A]
    Where XX is a hex pair, : is the starting 3A, and [0D][0A] is the ending.
    """
    try:
        spaced_lines = []
        
        for hex_line in hex_lines:
            spaced_parts = []
            
            # First element should be 3A (colon)
            if hex_line and hex_line[0].upper() == '3A':
                spaced_parts.append(': ')
            
            # Process middle elements
            for i in range(1, len(hex_line)):
                hex_code = hex_line[i].upper()
                
                # Check if this is start of [0D][0A] ending
                if i == len(hex_line) - 2 and hex_line[i].upper() == '0D' and hex_line[i+1].upper() == '0A':
                    spaced_parts.append('[0D] [0A]')
                    break  # Skip the next element as it's part of [0D] [0A]
                elif i == len(hex_line) - 1:
                    # Last element, if not part of [0D] [0A]
                    spaced_parts.append(hex_code)
                else:
                    #spaced_parts.append(hex_code)
                    spaced_parts.append(chr(int(hex_code, 16)))
                    if i & 1 == 0:
                        spaced_parts.append(' ')
                    #else:
                    #    spaced_parts.append(' '.join(chr(int(hex_code, 16))))
                    
            spaced_lines.append(''.join(spaced_parts))
        
        # Write spaced output file
        with open(output_file, 'w', encoding='utf-8') as f:
            for line in spaced_lines:
                f.write(line + '\n')
        
        return len(spaced_lines)
        
    except Exception as e:
        print(f"✗ Error creating spaced output: {e}")
        return 0


def main():
    """Main processing pipeline"""
    # Generate output filenames based on input filename
    if not os.path.exists(INPUT_FILE):
        print(f"✗ Input file not found: {INPUT_FILE}")
        return
    
    # Extract base name without extension
    base_name = os.path.splitext(INPUT_FILE)[0]
    hex_output_file = f'{base_name}_processed.txt'
    spaced_output_file = f'{base_name}_spaced.txt'
    
    print("=" * 60)
    print("Hex Log Processing Pipeline")
    print("=" * 60)
    
    # Step 1: Process hex file
    print(f"\n[1/2] Processing: {INPUT_FILE}")
    hex_lines, hex_count = process_log_file(INPUT_FILE, hex_output_file)
    
    if hex_count == 0:
        return
    
    print(f"✓ Extracted {hex_count} hex pairs")
    print(f"✓ Created {len(hex_lines)} separated lines")
    print(f"✓ Output saved to: {hex_output_file}")
    
    # Step 2: Create spaced representation
    print(f"\n[2/2] Creating spaced pair output...")
    spaced_count = create_spaced_output(hex_lines, spaced_output_file)
    
    if spaced_count == 0:
        return
    
    print(f"✓ Created {spaced_count} spaced lines")
    print(f"✓ Output saved to: {spaced_output_file}")
    
    print("\n" + "=" * 60)
    print("✓ Pipeline complete!")
    print("=" * 60)


if __name__ == '__main__':
    main()
