import argparse

def polute_bin_file(input_bin: str, output_bin: str):
    with open(input_bin, 'rb') as f:
        data = bytearray(f.read())

    data[100] = 0xFF
    data[101] = 0xFF
    data[102] = 0xFF
    data[103] = 0xFF

    # Write the modified data back to the file
    with open(output_bin, 'wb') as f:
        f.write(data)

def strip_bin_file(input_bin: str, output_bin: str, strip_len: str):
    prefix_size = 32 + 512 + 4
    with open(input_bin, "rb") as f:
        f.seek(prefix_size)
        data = f.read()
        data = data[0:int(strip_len)]

    with open(output_bin, "wb") as f:
        f.write(data)


def create_bin_file_with_length(input_wasm_path, output_bin_path):
    # Read the .wasm file
    with open(input_wasm_path, "rb") as wasm_file:
        wasm_data = wasm_file.read()

    wasm_size = len(wasm_data)

    if wasm_size > 0xFFFFFFFF:
        raise ValueError("WASM file is too large to fit in a 4-byte size field.")

    # Write the size and .wasm data to the output binary file
    with open(output_bin_path, "wb") as output_file:
        output_file.write(wasm_size.to_bytes(4, byteorder="little"))
        output_file.write(wasm_data)

    print(f"Created binary file: {output_bin_path}")
    print(f"WASM file size: {wasm_size} bytes")


def merge_binaries(bin1_path, bin2_path, output_path):
    with open(bin1_path, 'rb') as f1:
        bin1 = f1.read()

    with open(bin2_path, 'rb') as f2:
        bin2 = f2.read()

    # Pad the first binary to 0x20000 (if necessary)
    if len(bin1) > 0x20000:
        raise ValueError(f"First binary is larger than 0x20000 bytes ({len(bin1)} bytes).")

    padding_size = 0x20000 - len(bin1)
    padded_bin1 = bin1 + b'\xFF' * padding_size

    # Merge them
    merged = padded_bin1 + bin2

    # Write the result
    with open(output_path, 'wb') as f_out:
        f_out.write(merged)

    print(f"Merged binary saved to {output_path}.")


def main():
    parser = argparse.ArgumentParser(
        description="Create a binary file with the length of a .wasm file as the first word."
    )

    parser.add_argument("-s", "--strip", help="Strip binary")
    parser.add_argument("-m", "--merge", help="Merge two binaries")
    parser.add_argument("-c", "--create", action="store_true", help="Create binary")
    parser.add_argument("-p", "--polute", action="store_true", help="Polute binary")

    parser.add_argument("-i", "--input", help="Path to the input file")
    parser.add_argument("-o", "--output_bin", help="Path to the output binary file")

    args = parser.parse_args()

    if args.strip:
        strip_bin_file(args.input, args.output_bin, args.strip)
    if args.create:
        create_bin_file_with_length(args.input, args.output_bin)
    if args.polute:
        polute_bin_file(args.input, args.output_bin)
    if args.merge:
        merge_binaries(args.input, args.merge, args.output_bin)

if __name__ == "__main__":
    main()
