#!/usr/bin/env python3
"""
VGM to C Array Converter
Extracts note data from VGM file and converts to PSG-compatible format
"""

import struct
import sys

def parse_vgm(filename):
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Parse header
    version = struct.unpack('<I', data[0x08:0x0C])[0]
    total_samples = struct.unpack('<I', data[0x18:0x1C])[0]
    loop_offset = struct.unpack('<I', data[0x1C:0x20])[0]
    
    if version >= 0x150:
        vgm_data_offset = struct.unpack('<I', data[0x34:0x38])[0] + 0x34
    else:
        vgm_data_offset = 0x40
    
    print(f"// VGM: {total_samples/44100:.2f} seconds, loop at {loop_offset:#x}")
    
    pos = vgm_data_offset
    notes = []
    current_time = 0
    
    # Channel state tracking
    channels = [{'freq': 0, 'vol': 15, 'on': False} for _ in range(8)]
    
    while pos < len(data):
        cmd = data[pos]
        
        # Wait commands
        if cmd == 0x61:  # Wait n samples
            wait = struct.unpack('<H', data[pos+1:pos+3])[0]
            current_time += wait
            pos += 3
        elif cmd == 0x62:  # Wait 735 samples (1/60 sec)
            current_time += 735
            pos += 1
        elif cmd == 0x63:  # Wait 882 samples (1/50 sec)
            current_time += 882
            pos += 1
        elif cmd >= 0x70 and cmd <= 0x7F:  # Wait 1-16 samples
            current_time += (cmd & 0x0F) + 1
            pos += 1
        
        # YM2610 commands
        elif cmd == 0x52:  # YM2610 port 0
            reg = data[pos+1]
            val = data[pos+2]
            pos += 3
            
        elif cmd == 0x53:  # YM2610 port 1
            reg = data[pos+1]
            val = data[pos+2]
            pos += 3
            
        # YM2612 commands  
        elif cmd == 0x52 or cmd == 0x53:
            pos += 3
            
        # Other chip commands
        elif cmd in [0x90, 0x91, 0x92, 0x93, 0x94, 0x95]:
            pos += 5
            
        # End of data
        elif cmd == 0x66:
            break
            
        else:
            pos += 1
    
    return notes

def extract_simple_melody(filename):
    """Extract a simplified melody for PSG playback"""
    
    # For now, create a simple example melody
    # This would need proper VGM command parsing for your specific chip
    
    print("// Simple melody extracted (placeholder)")
    print("// TODO: Parse actual YM2610 commands to extract note data")
    print()
    print("typedef struct {")
    print("    uint16_t frequency;  // Hz")
    print("    uint8_t duration;    // frames (60 Hz)")
    print("    uint8_t volume;      // 0-15")
    print("} music_note_t;")
    print()
    print("static const music_note_t title_music[] = {")
    print("    // Format: {frequency, duration, volume}")
    print("    {440, 30, 12},  // A4 for 0.5 sec")
    print("    {494, 30, 12},  // B4")
    print("    {523, 30, 12},  // C5")
    print("    {0, 0, 0}       // End marker")
    print("};")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: parse_vgm.py <vgm_file>")
        sys.exit(1)
    
    extract_simple_melody(sys.argv[1])
