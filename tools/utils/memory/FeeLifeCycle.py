#!/usr/bin/env python3
# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021-2026 Parai Wang <parai@foxmail.com>
#
# Fee Flash Life Cycle Calculator

import os
import sys
import json
from typing import Optional, Dict, Any

__args = None

# Default configuration values (if not specified in NvM.json)
DEFAULT_NUMBER_OF_WRITE_CYCLES = 10000000  # 10 million write cycles per block
DEFAULT_BANK_SIZE = 32 * 1024  # 32KB per bank (typical for embedded Flash)
DEFAULT_PAGE_SIZE = 8  # Flash page size in bytes
DEFAULT_ADMIN_SIZE = 16  # sizeof(Fee_BlockType) = 16 bytes


def align(size, align_size=None):
    if align_size == None:
        align_size = __args.page_size
    return (size + align_size - 1) & (~(align_size - 1))


def alignData(size):
    return align(align(size, 2) + 4)


def getDataSize(data: Dict[str, Any]) -> int:
    typeInfo = {
        "uint8": 1,
        "uint16": 2,
        "uint32": 4,
        "uint64": 8,
        "uint8_n": lambda x: x.get("size", 1),
        "uint16_n": lambda x: x.get("size", 1) * 2,
        "uint32_n": lambda x: x.get("size", 1) * 4,
    }
    t = data["type"]
    if t.endswith("_n"):
        return typeInfo[t](data) * data.get("repeat", 1)
    return typeInfo.get(t, 1) * data.get("repeat", 1)


def getBlockSize(block: Dict[str, Any]) -> int:
    size = 0
    for data in block["data"]:
        size += getDataSize(data) * block.get("size", 1)
    return size


def calculateBackupRounds(cfgPath, verbose, bankSize, pageSize, numOfBanks):
    with open(cfgPath, "r") as f:
        cfg = json.load(f)

    blocks = cfg.get("blocks", [])

    adminSize = align(DEFAULT_ADMIN_SIZE)

    totalAdminPerBlock = 0
    totalDataPerBlock = 0
    blockDetails = []

    for block in blocks:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            blockSize = getBlockSize(block)
            dataSize = alignData(blockSize)
            numberOfWriteCycles = block.get("NumberOfWriteCycles", DEFAULT_NUMBER_OF_WRITE_CYCLES)

            blockDetails.append(
                {
                    "name": name,
                    "size": blockSize,
                    "data_size": dataSize,
                    "number_of_write_cycles": numberOfWriteCycles,
                }
            )

            totalAdminPerBlock += adminSize
            totalDataPerBlock += dataSize

    # Total space required to store one complete set of all blocks
    totalSpacePerBlock = totalAdminPerBlock + totalDataPerBlock

    maxDataSize = max(block["data_size"] for block in blockDetails)
    feeMinFreeSpace = maxDataSize + adminSize

    # Effective bank size for storing block copies
    # After backup, there must be FEE_MIN_FREE_SPACE available for new writes
    effectiveBankSize = bankSize - feeMinFreeSpace
    remainingSpaceAfterBackup = effectiveBankSize - totalSpacePerBlock

    if effectiveBankSize <= 0:
        print(
            f"Error: Bank size {bankSize} is too small. "
            f"Need at least {feeMinFreeSpace} bytes for minimum free space.",
            file=sys.stderr,
        )
        return None

    # Scenario A: Sum all blocks' rounds, then divide by numOfBanks (per-bank rounds).
    maxBackupRoundsByWriteCycles = 0
    blockRoundDetails = []

    for block in blockDetails:
        blockSpace = adminSize + block["data_size"]
        firstPhaseWrites = effectiveBankSize // blockSpace
        subsequentWrites = remainingSpaceAfterBackup // blockSpace if remainingSpaceAfterBackup > 0 else 0
        rounds = ((block["number_of_write_cycles"] - firstPhaseWrites - 1) // subsequentWrites) + 1
        blockRoundDetails.append(
            {
                "name": block["name"],
                "block_space": blockSpace,
                "first_phase_writes": firstPhaseWrites,
                "subsequent_writes": subsequentWrites,
                "write_cycles": block["number_of_write_cycles"],
                "rounds": rounds,
            }
        )
        maxBackupRoundsByWriteCycles += rounds
    maxBackupRoundsByWriteCycles = maxBackupRoundsByWriteCycles // numOfBanks

    # Scenario B: Accumulated total data across all blocks
    # Only calculate if there's remaining space and total data exceeds first phase capacity
    totalDataWritten = 0
    for block in blockDetails:
        blockSpace = adminSize + block["data_size"]
        totalDataWritten += blockSpace * block["number_of_write_cycles"]
    maxBackupRoundsByAccumulated = ((totalDataWritten - effectiveBankSize - 1) // remainingSpaceAfterBackup) + 1
    maxBackupRoundsByAccumulated = maxBackupRoundsByAccumulated // numOfBanks

    # Take the worst case: max of per-block max and accumulated total
    maxBackupRounds = max(maxBackupRoundsByWriteCycles, maxBackupRoundsByAccumulated)

    # Verbose output
    if verbose:
        maxNameLen = max(len(block["name"]) for block in blockDetails) if blockDetails else 20
        print("=" * 70)
        print("FEE Flash Life Cycle Analysis")
        print("=" * 70)
        print(f"Number of Banks: {numOfBanks}")
        print(f"Bank Size: {bankSize} bytes")
        print(f"Page Size: {pageSize} bytes")
        print(f"Admin Size (per block): {adminSize} bytes")
        print("-" * 70)
        print(f"{'Block Name':<{maxNameLen}} {'Size':>8} {'Data Size':>10} {'Write Cycles':>15}")
        print("-" * 70)
        for block in blockDetails:
            print(
                f"{block['name']:<{maxNameLen}} {block['size']:>8} {block['data_size']:>10} "
                f"{block['number_of_write_cycles']:>15,}"
            )
        print("-" * 70)
        print(f"Total Admin per block set: {totalAdminPerBlock} bytes")
        print(f"Total Data per block set: {totalDataPerBlock} bytes")
        print(f"Total Space per block set: {totalSpacePerBlock} bytes")
        print(f"FEE_MIN_FREE_SPACE: {feeMinFreeSpace} bytes (for new writes after backup)")
        print(f"Effective Bank Size: {effectiveBankSize} bytes")
        print(f"Remaining Space After Backup: {remainingSpaceAfterBackup} bytes")
        print(f"Total Flash Space: {numOfBanks * bankSize:,} bytes")
        print(f"Theoretical Max Writes (no backup): {(numOfBanks * bankSize) // totalSpacePerBlock:,}")
        print("-" * 70)
        print(f"Per-Block Backup Round Analysis:")
        print("-" * 70)
        print(
            f"{'Block Name':<{maxNameLen}} {'Block Space':>12} {'1st Phase':>10} "
            f"{'Subsequent':>12} {'Write Cycles':>12} {'Rounds':>10}"
        )
        print("-" * 70)
        for detail in blockRoundDetails:
            print(
                f"{detail['name']:<{maxNameLen}} {detail['block_space']:>12} "
                f"{detail['first_phase_writes']:>10} {detail['subsequent_writes']:>12} "
                f"{detail['write_cycles']:>12,} {detail['rounds']:>10}"
            )
        print("-" * 70)
        print(f"Accumulated Total Data Written: {totalDataWritten:,} bytes")
        print(f"  First Phase Capacity: {effectiveBankSize:,} bytes")
        print(f"  Subsequent Phase Capacity: {remainingSpaceAfterBackup:,} bytes")
        print(f"Per-Block Sum (per bank): {maxBackupRoundsByWriteCycles:,}")
        print(f"Accumulated (per bank): {maxBackupRoundsByAccumulated:,}")

    return {
        "bank_size": bankSize,
        "page_size": pageSize,
        "max_backup_rounds_by_write_cycles": maxBackupRoundsByWriteCycles,
        "max_backup_rounds_by_accumulated": maxBackupRoundsByAccumulated,
        "max_backup_rounds": maxBackupRounds,
        "block_details": blockDetails,
    }


def main():
    global __args
    import argparse

    parser = argparse.ArgumentParser(
        description="FEE Flash Life Cycle Calculator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
This tool calculates the worst-case backup rounds for FEE configuration
to ensure Flash persistence for 10 years in vehicle applications.

Key Parameters:
  - NumberOfWriteCycles: Max write cycles per block (default: 10,000,000)
  - bank_size: Bank size in bytes (default: 32768 = 32KB)
  - num_of_banks: Number of banks (2 or 4, default: 2)

Example:
  python FeeLifeCycle.py app/app/config/NvM/NvM.json -v
  python FeeLifeCycle.py app/app/config/NvM/NvM.json --block_size "64*1024" --page_size 8
  python FeeLifeCycle.py app/app/config/NvM/NvM.json --num_of_banks 4 -v
        """,
    )
    parser.add_argument("config", help="Path to NvM.json configuration file")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    parser.add_argument("--block_size", help="bank size", default="32*1024", type=str, required=False)
    parser.add_argument("--page_size", help="page size", default=8, type=int, required=False)
    parser.add_argument("--num_of_banks", help="number of banks", default=2, type=int, required=False)
    args = parser.parse_args()
    __args = args
    # Evaluate block_size expression if provided (supports "32*1024" style)
    if args.block_size is not None:
        # Safely parse numeric expressions (only digits, spaces, *, +, -, /, (, ))
        if not all(c in "0123456789*+-/() " for c in args.block_size):
            print(f"Error: Invalid block_size expression: {args.block_size}", file=sys.stderr)
            sys.exit(1)
        args.block_size = eval(args.block_size, {"__builtins__": {}}, {})

    if not os.path.exists(args.config):
        print(f"Error: Configuration file not found: {args.config}", file=sys.stderr)
        sys.exit(1)

    result = calculateBackupRounds(args.config, args.verbose, args.block_size, args.page_size, args.num_of_banks)

    if result:
        max_backup_rounds = result["max_backup_rounds"]
        print("-" * 70)
        print(f"MAXIMUM BACKUP ROUNDS (Worst Case): {max_backup_rounds:,}")
        print("=" * 70)


if __name__ == "__main__":
    main()
