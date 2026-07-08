import struct
import time
import argparse
import random
import string
import os

def generate_random_name():
    # Flipper Zero names usually look like "Artemis" or "Ronaldo" (capitalized, max 8 chars)
    # Let's generate a random 8-character string for fun, starting with a capital letter
    first_char = random.choice(string.ascii_uppercase)
    rest_chars = ''.join(random.choices(string.ascii_lowercase + string.digits, k=7))
    return first_char + rest_chars

def main():
    parser = argparse.ArgumentParser(description="Generate 32-byte OTP binary file for STM32WB55 (Flipper Zero DIY)")
    
    parser.add_argument("-n", "--name", type=str, default=None,
                        help="Device Name (exactly 8 characters, e.g. 'Ronaldo7')")
    parser.add_argument("-v", "--version", type=int, default=12,
                        help="Board Version (default: 12)")
    parser.add_argument("-t", "--target", type=int, default=7,
                        help="Board Target Firmware (default: 7)")
    parser.add_argument("-b", "--body", type=int, default=9,
                        help="Board Body (default: 9)")
    parser.add_argument("-c", "--connect", type=int, default=6,
                        help="Board Interconnect (default: 6)")
    parser.add_argument("-d", "--display", type=int, default=2,
                        help="Board Display: 1=ERC, 2=MGG (default: 2 - Monochrome Glass Grid)")
    parser.add_argument("-col", "--color", type=int, default=1,
                        help="Board Color: 1=Black, 2=White, 3=Transparent (default: 1)")
    parser.add_argument("-r", "--region", type=int, default=1,
                        help="Board Region: 1=EuRu, 2=UsCaAu, 3=Jp, 4=World (default: 1)")
    parser.add_argument("-o", "--output", type=str, default="flipper_otp.bin",
                        help="Output binary file path (default: flipper_otp.bin)")
    
    args = parser.parse_args()
    
    # Process Name
    name = args.name
    if name is None:
        name = generate_random_name()
        print(f"[*] No name provided. Generated random name: '{name}'")
    
    if len(name) > 8:
        print(f"[!] Warning: Name '{name}' is longer than 8 characters. Truncating to '{name[:8]}'")
        name = name[:8]
    elif len(name) < 8:
        # Pad with null bytes if name is shorter than 8 chars
        name = name.ljust(8, '\x00')
    
    # Convert name to bytes
    name_bytes = name.encode('ascii', errors='ignore')
    
    # Constants
    header_magic = 0xBABE
    header_version = 2
    header_reserved = 0
    header_timestamp = int(time.time())
    
    board_reserved2_0 = 0
    board_reserved2_1 = 0
    
    board_reserved3_0 = 0
    board_reserved3_1 = 0
    
    # Pack OTP V2 Structure (32 bytes total)
    # Format description:
    # < - Little Endian
    # H - uint16 (header_magic)
    # B - uint8  (header_version)
    # B - uint8  (header_reserved)
    # I - uint32 (header_timestamp)
    #
    # B - uint8  (board_version)
    # B - uint8  (board_target)
    # B - uint8  (board_body)
    # B - uint8  (board_connect)
    # B - uint8  (board_display)
    # B - uint8  (board_reserved2_0)
    # H - uint16 (board_reserved2_1)
    #
    # B - uint8  (board_color)
    # B - uint8  (board_region)
    # H - uint16 (board_reserved3_0)
    # I - uint32 (board_reserved3_1)
    #
    # 8s - char[8] (name)
    otp_data = struct.pack(
        "<HBBI BBBBBBH BBHI 8s",
        header_magic,
        header_version,
        header_reserved,
        header_timestamp,
        
        args.version,
        args.target,
        args.body,
        args.connect,
        args.display,
        board_reserved2_0,
        board_reserved2_1,
        
        args.color,
        args.region,
        board_reserved3_0,
        board_reserved3_1,
        
        name_bytes
    )
    
    # Write to output file
    output_path = args.output
    with open(output_path, "wb") as f:
        f.write(otp_data)
        
    print(f"[+] Successfully generated OTP V2 binary file!")
    print(f"    - Output Path:    {os.path.abspath(output_path)}")
    print(f"    - Name:           '{name.strip(chr(0))}'")
    print(f"    - Board Version:  {args.version}")
    # Display type string mapping
    display_str = "MGG (Monochrome Glass Grid)" if args.display == 2 else ("ERC" if args.display == 1 else "Unknown")
    print(f"    - Display Type:   {args.display} ({display_str})")
    print(f"    - Region Code:    {args.region}")
    print(f"    - Timestamp:      {header_timestamp} ({time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(header_timestamp))})")
    print(f"    - Size:           {len(otp_data)} bytes")
    print("\nFlashing Instructions:")
    print(f"   1. Connect your STM32WB55 board in DFU mode (hold BOOT0 and plug in USB).")
    print(f"   2. Open STM32CubeProgrammer and connect via USB.")
    print(f"   3. Go to the 'Erasing & Programming' tab.")
    print(f"   4. Browse for the generated file: '{os.path.abspath(output_path)}'")
    print(f"   5. Set the Start Address to: 0x1FFF7000")
    print(f"   6. Click 'Start Programming'.")

if __name__ == "__main__":
    main()
