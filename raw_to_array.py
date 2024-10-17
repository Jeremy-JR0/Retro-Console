# raw_to_array.py
import sys

if len(sys.argv) != 2:
    print("Usage: python raw_to_array.py <input.raw>")
    sys.exit(1)

input_file = sys.argv[1]
output_file = input_file.replace('.raw', '.h')

with open(input_file, 'rb') as f:
    data = f.read()

with open(output_file, 'w') as f:
    f.write('const uint8_t soundData[] = {\n')
    for i, byte in enumerate(data):
        f.write(f'0x{byte:02X}, ')
        if (i + 1) % 16 == 0:
            f.write('\n')
    f.write('\n};\n')
    f.write(f'const unsigned int soundDataLength = {len(data)};\n')
