#!/usr/bin/env python3
"""
Furnace Tracker Text Export to C Music Data Converter
Converts Furnace .txt export to C arrays for RP6502 PSG playback
"""

import re
import sys

# Note to frequency mapping (in Hz) for PSG
NOTE_FREQ = {
    'C-0': 16, 'C#0': 17, 'D-0': 18, 'D#0': 19, 'E-0': 21, 'F-0': 22, 'F#0': 23, 'G-0': 25, 'G#0': 26, 'A-0': 28, 'A#0': 29, 'B-0': 31,
    'C-1': 33, 'C#1': 35, 'D-1': 37, 'D#1': 39, 'E-1': 41, 'F-1': 44, 'F#1': 46, 'G-1': 49, 'G#1': 52, 'A-1': 55, 'A#1': 58, 'B-1': 62,
    'C-2': 65, 'C#2': 69, 'D-2': 73, 'D#2': 78, 'E-2': 82, 'F-2': 87, 'F#2': 93, 'G-2': 98, 'G#2': 104, 'A-2': 110, 'A#2': 117, 'B-2': 123,
    'C-3': 131, 'C#3': 139, 'D-3': 147, 'D#3': 156, 'E-3': 165, 'F-3': 175, 'F#3': 185, 'G-3': 196, 'G#3': 208, 'A-3': 220, 'A#3': 233, 'B-3': 247,
    'C-4': 262, 'C#4': 277, 'D-4': 294, 'D#4': 311, 'E-4': 330, 'F-4': 349, 'F#4': 370, 'G-4': 392, 'G#4': 415, 'A-4': 440, 'A#4': 466, 'B-4': 494,
    'C-5': 523, 'C#5': 554, 'D-5': 587, 'D#5': 622, 'E-5': 659, 'F-5': 698, 'F#5': 740, 'G-5': 784, 'G#5': 831, 'A-5': 880, 'A#5': 932, 'B-5': 988,
    'C-6': 1047, 'C#6': 1109, 'D-6': 1175, 'D#6': 1245, 'E-6': 1319, 'F-6': 1397, 'F#6': 1480, 'G-6': 1568, 'G#6': 1661, 'A-6': 1760, 'A#6': 1865, 'B-6': 1976,
    'OFF': 0,  # Note off
    '...': None,  # Empty/continue
}

def parse_furnace_text(filename):
    """Parse Furnace text export and extract note sequences"""
    
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    # Find where patterns start
    pattern_start = None
    for i, line in enumerate(lines):
        if '## Patterns' in line:
            pattern_start = i
            break
    
    if pattern_start is None:
        print("Error: Could not find pattern data")
        return None
    
    # Parse pattern data
    channels = {}  # channel_id -> list of (row, note, volume)
    current_order = None
    current_row = 0
    
    for line in lines[pattern_start:]:
        # Check for order marker
        order_match = re.match(r'----- ORDER (\d+)', line)
        if order_match:
            current_order = int(order_match.group(1))
            current_row = 0
            continue
        
        # Parse row data: "00 |C-3 00 70 ....|... .. .. ....|C-3 04 7D ...."
        row_match = re.match(r'([0-9A-F]{2}) \|(.+)', line)
        if row_match and current_order is not None:
            row_hex = row_match.group(1)
            current_row = int(row_hex, 16)
            
            # Split by | to get each channel
            channel_data = row_match.group(2).split('|')
            
            for ch_idx, ch in enumerate(channel_data):
                # Parse: "C-3 04 7D ...." or "... .. .. ...."
                parts = ch.strip().split()
                if len(parts) >= 3:
                    note = parts[0]  # e.g., "C-3" or "..."
                    inst = parts[1]  # instrument
                    vol = parts[2]   # volume (hex)
                    
                    if note != '...':  # Only record actual notes
                        if ch_idx not in channels:
                            channels[ch_idx] = []
                        
                        # Convert volume from hex (0-7F range) to 0-15
                        try:
                            vol_int = int(vol, 16) if vol != '..' else 127
                            vol_scaled = min(15, vol_int // 8)
                        except:
                            vol_scaled = 12
                        
                        freq = NOTE_FREQ.get(note, 0)
                        if freq is not None:  # Skip '...' entries
                            channels[ch_idx].append({
                                'order': current_order,
                                'row': current_row,
                                'note': note,
                                'freq': freq,
                                'volume': vol_scaled,
                                'instrument': inst
                            })
    
    return channels

def convert_to_music_data(channels, target_channel=2):
    """Convert channel data to simple music sequence
    
    Args:
        channels: dict of channel data from parse_furnace_text
        target_channel: which Furnace channel to extract (default 2 for SN76489)
    """
    
    if target_channel not in channels:
        print(f"Warning: Channel {target_channel} not found")
        return []
    
    notes = channels[target_channel]
    
    # Convert to music sequence with durations
    sequence = []
    for i, note in enumerate(notes):
        # Calculate duration until next note (or default to 4 rows = ~4 frames at 60Hz)
        if i < len(notes) - 1:
            next_note = notes[i + 1]
            duration = (next_note['row'] - note['row']) + (next_note['order'] - note['order']) * 64
        else:
            duration = 4
        
        # Limit duration
        duration = min(duration, 60)  # Max 1 second
        if duration < 1:
            duration = 1
        
        sequence.append({
            'freq': note['freq'],
            'duration': duration,
            'volume': note['volume']
        })
    
    return sequence

def generate_c_code(sequence, var_name="title_music"):
    """Generate C code for music data"""
    
    print(f"// Auto-generated from Furnace Tracker export")
    print(f"// Total notes: {len(sequence)}")
    print()
    print("typedef struct {")
    print("    uint16_t frequency;  // Hz")
    print("    uint8_t duration;    // frames (60 Hz)")
    print("    uint8_t volume;      // 0-15")
    print("} music_note_t;")
    print()
    print(f"static const music_note_t {var_name}[] = {{")
    print("    // Format: {frequency, duration, volume}")
    
    for note in sequence:
        if note['freq'] == 0:
            print(f"    {{0, {note['duration']}, 0}},  // REST")
        else:
            print(f"    {{{note['freq']}, {note['duration']}, {note['volume']}}},")
    
    print("    {0, 0, 0}  // End marker")
    print("};")
    print()
    print(f"#define {var_name.upper()}_LENGTH {len(sequence)}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: furnace_to_c.py <furnace_text_file> [channel_number]")
        print("  channel_number: which channel to extract (default: 2)")
        sys.exit(1)
    
    filename = sys.argv[1]
    target_channel = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    
    print(f"// Parsing {filename}, extracting channel {target_channel}", file=sys.stderr)
    
    channels = parse_furnace_text(filename)
    if channels:
        print(f"// Found {len(channels)} channels", file=sys.stderr)
        sequence = convert_to_music_data(channels, target_channel)
        generate_c_code(sequence)
