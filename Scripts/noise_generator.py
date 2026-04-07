#!/usr/bin/env python3
"""
Fast seamlessly tiling Simplex noise generator using C-accelerated library.
Requires: pip install noise pillow
"""

import argparse
import numpy as np
from PIL import Image

try:
    import noise
except ImportError:
    print("Error: noise library required. Install with:")
    print("  pip install noise pillow")
    exit(1)


def generate_tiling_noise(width, height, frequency, seed, octaves, persistence, lacunarity, use_3d=False):
    """
    Generate seamlessly tiling Perlin noise (2D or 3D).
    Uses C-accelerated noise library for speed.
    """
    noise_array = np.zeros((height, width))

    if use_3d:
        # 3D noise sampled on torus surface
        for y in range(height):
            for x in range(width):
                nx = x / width
                ny = y / height

                # Map to torus angles
                angle_u = nx * 2 * np.pi
                angle_v = ny * 2 * np.pi

                # 3D position on torus
                R = 2.0
                r = 1.0
                px = (R + r * np.cos(angle_v)) * np.cos(angle_u) * frequency
                py = (R + r * np.cos(angle_v)) * np.sin(angle_u) * frequency
                pz = r * np.sin(angle_v) * frequency

                n = noise.pnoise3(px, py, pz, octaves=octaves, persistence=persistence,
                                 lacunarity=lacunarity, repeatx=1024, repeaty=1024, repeatz=1024,
                                 base=seed)
                noise_array[y, x] = n
    else:
        # Simple 2D noise
        for y in range(height):
            for x in range(width):
                n = noise.pnoise2(x * frequency / width, y * frequency / height,
                                 octaves=octaves, persistence=persistence,
                                 lacunarity=lacunarity, repeatx=width, repeaty=height,
                                 base=seed)
                noise_array[y, x] = n

    # Normalize to [0, 1]
    noise_array = (noise_array + 1.0) / 2.0
    noise_array = np.clip(noise_array, 0, 1)

    return noise_array


def main():
    parser = argparse.ArgumentParser(
        description='Fast seamlessly tiling Simplex noise generator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  py noise_generator.py                                # Default 512x512
  py noise_generator.py --frequency 3.0               # More detail
  py noise_generator.py --octaves 6 --3d              # High detail 3D
  py noise_generator.py --frequency 0.5 -o terrain.png
        '''
    )

    parser.add_argument('--width', type=int, default=512,
                       help='Texture width (default: 512)')
    parser.add_argument('--height', type=int, default=512,
                       help='Texture height (default: 512)')
    parser.add_argument('--frequency', type=float, default=2.0,
                       help='Noise frequency - higher = more detail, lower = larger features (default: 2.0)')
    parser.add_argument('--seed', type=int, default=42,
                       help='Random seed (default: 42)')
    parser.add_argument('--octaves', type=int, default=4,
                       help='Fractal octaves - more detail (default: 4)')
    parser.add_argument('--persistence', type=float, default=0.5,
                       help='Amplitude decay per octave (default: 0.5)')
    parser.add_argument('--lacunarity', type=float, default=2.0,
                       help='Frequency multiplier per octave (default: 2.0)')
    parser.add_argument('--output', type=str, default='noise.png',
                       help='Output filename (default: noise.png)')
    parser.add_argument('--3d', action='store_true', dest='use_3d',
                       help='Use 3D noise (torus-mapped) instead of 2D')

    args = parser.parse_args()

    print(f"Generating {args.width}x{args.height} tiling {'3D' if args.use_3d else '2D'} noise...")
    noise_array = generate_tiling_noise(
        args.width, args.height,
        args.frequency, args.seed,
        args.octaves, args.persistence, args.lacunarity,
        args.use_3d
    )

    image_array = (noise_array * 255).astype(np.uint8)
    image = Image.fromarray(image_array, mode='L')
    image.save(args.output)
    print(f"✓ Saved to {args.output}")


if __name__ == '__main__':
    main()
