import sys


# Function to get the S-Record type (e.g., 'S0', 'S1', 'S3', ...)
def s19_record_type(line):
    return line[0:2]  # First two characters indicate the record type (S0, S1, S3...)


# Function to get the byte count field from the S-Record line
def s19_byte_count(line):
    return int(line[2:4], 16)  # Byte count is a hex number after 'Sx'


# Function to extract the address from the S-Record based on its type
def s19_address(line, record_type):
    if record_type == "S1":
        # S1 record: 2-byte address (4 hex chars)
        addr = int(line[4:8], 16)
        return addr
    elif record_type == "S2":
        # S2 record: 3-byte address (6 hex chars)
        addr = int(line[4:10], 16)
        return addr
    elif record_type == "S3":
        # S3 record: 4-byte address (8 hex chars, 32-bit)
        addr = int(line[4:12], 16)
        return addr
    else:
        # For S0, S5, S7, S8, S9: no meaningful address
        return 0


# Function to extract the data bytes from the S-Record
def s19_data(line, record_type):
    if record_type == "S1":
        # Data starts after byte count (2) + address (2) => data starts at index 4+2=6
        data_start = 4 + 4
        data_byte_count = s19_byte_count(line) - 3  # Total byte count - (1 byte count + 2 address + 1 checksum)
        data_hex = line[data_start : data_start + 2 * data_byte_count]
    elif record_type == "S2":
        data_start = 4 + 6
        data_byte_count = s19_byte_count(line) - 4
        data_hex = line[data_start : data_start + 2 * data_byte_count]
    elif record_type == "S3":
        data_start = 4 + 8
        data_byte_count = s19_byte_count(line) - 5
        data_hex = line[data_start : data_start + 2 * data_byte_count]
    else:
        data_hex = ""
    # Convert every 2 hex chars to one byte (as integer)
    data_bytes = [int(data_hex[i : i + 2], 16) for i in range(0, len(data_hex), 2)]
    return data_bytes


# Function to extract raw address bytes (for reconstructing the new record)
def s19_address_bytes(line, record_type):
    if record_type == "S1":
        addr_bytes = bytes.fromhex(line[4:8])
    elif record_type == "S2":
        addr_bytes = bytes.fromhex(line[4:10])
    elif record_type == "S3":
        addr_bytes = bytes.fromhex(line[4:14])
    else:
        addr_bytes = b""
    return addr_bytes


# Function to compute the checksum of an S-Record line (for reference or validation)
def compute_checksum(line):
    bytes_list = []
    for i in range(2, len(line), 2):  # Skip record type (e.g., 'S3'), start from byte count
        byte = int(line[i : i + 2], 16)
        bytes_list.append(byte)
    return sum(bytes_list) & 0xFF


# Function to generate a new S-Record line with updated address and correct checksum
def generate_new_srecord(record_type, new_address, data_bytes):
    if record_type == "S1":
        addr_byte_len = 2  # 2-byte address
    elif record_type == "S2":
        addr_byte_len = 3  # 3-byte address
    elif record_type == "S3":
        addr_byte_len = 4  # 4-byte address
    else:
        raise ValueError("Unsupported S-Record type")

    # Total byte count = 1 (byte count field) + addr_bytes + data_bytes + 1 (checksum)
    byte_count = 1 + addr_byte_len + len(data_bytes)

    # Convert new address to bytes
    if record_type == "S1":
        addr_bytes = new_address.to_bytes(2, byteorder="big")
    elif record_type == "S2":
        addr_bytes = new_address.to_bytes(3, byteorder="big")
    elif record_type == "S3":
        addr_bytes = new_address.to_bytes(4, byteorder="big")

    # Data bytes are already in list format
    all_bytes = [byte_count]
    all_bytes += list(addr_bytes)
    all_bytes += data_bytes

    # Compute checksum: sum of all bytes including byte count, address, and data
    checksum = sum(all_bytes) & 0xFF
    checksum = (~checksum) & 0xFF  # One's complement

    # Build the new S-Record string
    srec = f"{record_type}"
    srec += f"{byte_count:02X}"  # Byte count as 2-digit hex
    srec += addr_bytes.hex().upper()  # Address in uppercase hex
    srec += "".join(f"{b:02X}" for b in data_bytes)  # Data bytes in hex
    srec += f"{checksum:02X}"  # Checksum
    return srec


# Function to process a single line of S-Record
def process_s19_line(line, remap):
    record_type = s19_record_type(line)
    if record_type not in ["S0", "S1", "S2", "S3", "S5", "S7", "S8", "S9"]:
        return None, line  # Unknown or unsupported record type, return as-is
    if record_type in ["S1", "S2", "S3"]:
        byte_count = s19_byte_count(line)
        old_addr = s19_address(line, record_type)
        data_bytes = s19_data(line, record_type)
        addr_bytes = s19_address_bytes(line, record_type)
        bInRemap = False
        for src_address, length, dst_address in remap:
            if old_addr >= src_address and old_addr < (src_address + length):
                address_offset = dst_address - src_address
                bInRemap = True
                break
        if not bInRemap:
            return old_addr, line  # Address not in any remap range, return as-is
        # Apply address offset
        new_addr = old_addr + address_offset

        # Generate new S-Record line with updated address
        new_line = generate_new_srecord(record_type, new_addr, data_bytes)
        return new_addr, new_line
    else:
        # For non-address records (like S0, S7, S9), return unchanged
        return None, line


# Function to remap an entire S19 file
def remap_s19_file(input_file, output_file, remap=[]):
    with open(input_file, "r", encoding="utf-8", errors="ignore") as fin, open(
        output_file, "w", encoding="utf-8"
    ) as fout:
        header = []
        foot = []
        data = []
        for line in fin:
            stripped_line = line.strip()
            if not stripped_line:
                continue  # Skip empty lines
            if stripped_line[0] in ["S", "s"]:  # Process only S-Records
                addr, new_line = process_s19_line(stripped_line, remap)
                if addr is not None:
                    data.append((addr, new_line))
                elif len(data) == 0:
                    header.append(line)
                else:
                    foot.append(line)
            else:
                # Keep non-S-Record lines as-is (e.g., comments, headers)
                raise ValueError("Invalid S-Record line encountered")
        for l in header:
            fout.write(l)
        # Sort data records by address
        data.sort(key=lambda x: x[0])
        for addr, l in data:
            fout.write(l + "\n")
        for l in foot:
            fout.write(l)
