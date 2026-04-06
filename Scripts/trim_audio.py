#!/usr/bin/env python3
"""Trim and convert audio files."""

import sys
from pathlib import Path
from pydub import AudioSegment

def trim_and_convert(input_file, output_file, start_ms, end_ms):
    """Trim audio and convert format."""
    input_path = Path(input_file)
    output_path = Path(output_file)

    if not input_path.exists():
        print(f"Error: {input_file} not found")
        sys.exit(1)

    print(f"Loading {input_file}...")
    audio = AudioSegment.from_mp3(str(input_path))

    print(f"Trimming from {start_ms}ms to {end_ms}ms...")
    trimmed = audio[start_ms:end_ms]

    print(f"Exporting to {output_file}...")
    trimmed.export(str(output_path), format="wav")

    print(f"Done! Duration: {len(trimmed) / 1000:.2f}s")

if __name__ == "__main__":
    # Convert seconds to milliseconds
    start_s = 2.9
    end_s = 5.7
    start_ms = int(start_s * 1000)
    end_ms = int(end_s * 1000)

    input_file = "Data/sounds/bike_free_wheel_source.mp3"
    output_file = "Data/sounds/free_wheel.wav"

    trim_and_convert(input_file, output_file, start_ms, end_ms)
