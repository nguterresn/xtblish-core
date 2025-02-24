import argparse

def create_bin_file_with_length(input_wasm_path, output_bin_path):
    # Read the .wasm file
    with open(input_wasm_path, "rb") as wasm_file:
        wasm_data = wasm_file.read()

    # Get the length of the .wasm file
    wasm_size = len(wasm_data)

    # Ensure the size fits in a 4-byte word
    if wasm_size > 0xFFFFFFFF:
        raise ValueError("WASM file is too large to fit in a 4-byte size field.")

    # Write the size and .wasm data to the output binary file
    with open(output_bin_path, "wb") as output_file:
        # Write the size as a 4-byte little-endian word
        output_file.write(wasm_size.to_bytes(4, byteorder="little"))
        # Write the .wasm file content
        output_file.write(wasm_data)

    print(f"Created binary file: {output_bin_path}")
    print(f"WASM file size: {wasm_size} bytes")


def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(
        description="Create a binary file with the length of a .wasm file as the first word."
    )
    parser.add_argument("-i", "--input_wasm", help="Path to the input .wasm file")
    parser.add_argument("-o", "--output_bin", help="Path to the output binary file")

    # Parse arguments
    args = parser.parse_args()

    # Create the binary file
    create_bin_file_with_length(args.input_wasm, args.output_bin)


if __name__ == "__main__":
    main()
