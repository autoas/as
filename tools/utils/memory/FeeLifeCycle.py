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
        align_size = __args.PageSize
    return (size + align_size - 1) & (~(align_size - 1))


def alignData(size):
    return align(align(size, 2) + 4)


def getDataSize(data: Dict[str, Any]) -> int:
    typeInfo = {
        "uint8": 1,
        "uint16": 2,
        "uint32": 4,
        "uint64": 8,
        "uint8_n": lambda x: x.get("Size", 1),
        "uint16_n": lambda x: x.get("Size", 1) * 2,
        "uint32_n": lambda x: x.get("Size", 1) * 4,
    }
    t = data["Type"]
    if t.endswith("_n"):
        return typeInfo[t](data) * data.get("Repeat", 1)
    return typeInfo.get(t, 1) * data.get("Repeat", 1)


def getBlockSize(block: Dict[str, Any]) -> int:
    size = 0
    for data in block["Data"]:
        size += getDataSize(data) * block.get("Size", 1)
    return size


def calculateBackupRounds(cfgPath, verbose, bankSize, pageSize, numOfBanks):
    with open(cfgPath, "r") as f:
        cfg = json.load(f)

    blocks = cfg.get("Blocks", [])

    adminSize = align(DEFAULT_ADMIN_SIZE)

    totalAdminPerBlock = 0
    totalDataPerBlock = 0
    blockDetails = []

    for block in blocks:
        repeat = block.get("Repeat", 1)
        for i in range(repeat):
            name = block["Name"].format(i)
            blockSize = getBlockSize(block)
            dataSize = alignData(blockSize)
            numberOfWriteCycles = block.get("NumberOfWriteCycles", DEFAULT_NUMBER_OF_WRITE_CYCLES)

            blockDetails.append(
                {
                    "Name": name,
                    "Size": blockSize,
                    "DataSize": dataSize,
                    "NumberOfWriteCycles": numberOfWriteCycles,
                }
            )

            totalAdminPerBlock += adminSize
            totalDataPerBlock += dataSize

    # Total space required to store one complete set of all blocks
    totalSpacePerBlock = totalAdminPerBlock + totalDataPerBlock

    maxDataSize = max(block["DataSize"] for block in blockDetails)
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
        blockSpace = adminSize + block["DataSize"]
        firstPhaseWrites = effectiveBankSize // blockSpace
        subsequentWrites = remainingSpaceAfterBackup // blockSpace if remainingSpaceAfterBackup > 0 else 0
        rounds = ((block["NumberOfWriteCycles"] - firstPhaseWrites - 1) // subsequentWrites) + 1
        blockRoundDetails.append(
            {
                "Name": block["Name"],
                "BlockSpace": blockSpace,
                "FirstPhaseWrites": firstPhaseWrites,
                "SubsequentWrites": subsequentWrites,
                "WriteCycles": block["NumberOfWriteCycles"],
                "Rounds": rounds,
            }
        )
        maxBackupRoundsByWriteCycles += rounds
    maxBackupRoundsByWriteCycles = maxBackupRoundsByWriteCycles // numOfBanks

    # Scenario B: Accumulated total data across all blocks
    # Only calculate if there's remaining space and total data exceeds first phase capacity
    totalDataWritten = 0
    for block in blockDetails:
        blockSpace = adminSize + block["DataSize"]
        totalDataWritten += blockSpace * block["NumberOfWriteCycles"]
    maxBackupRoundsByAccumulated = ((totalDataWritten - effectiveBankSize - 1) // remainingSpaceAfterBackup) + 1
    maxBackupRoundsByAccumulated = maxBackupRoundsByAccumulated // numOfBanks

    # Take the worst case: max of per-block max and accumulated total
    maxBackupRounds = max(maxBackupRoundsByWriteCycles, maxBackupRoundsByAccumulated)

    # Verbose output
    if verbose:
        maxNameLen = max(len(block["Name"]) for block in blockDetails) if blockDetails else 20
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
                f"{block['Name']:<{maxNameLen}} {block['Size']:>8} {block['DataSize']:>10} "
                f"{block['NumberOfWriteCycles']:>15,}"
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
                f"{detail['Name']:<{maxNameLen}} {detail['BlockSpace']:>12} "
                f"{detail['FirstPhaseWrites']:>10} {detail['SubsequentWrites']:>12} "
                f"{detail['WriteCycles']:>12,} {detail['Rounds']:>10}"
            )
        print("-" * 70)
        print(f"Accumulated Total Data Written: {totalDataWritten:,} bytes")
        print(f"  First Phase Capacity: {effectiveBankSize:,} bytes")
        print(f"  Subsequent Phase Capacity: {remainingSpaceAfterBackup:,} bytes")
        print(f"Per-Block Sum (per bank): {maxBackupRoundsByWriteCycles:,}")
        print(f"Accumulated (per bank): {maxBackupRoundsByAccumulated:,}")

    return {
        "BankSize": bankSize,
        "PageSize": pageSize,
        "MaxBackupRoundsByWriteCycles": maxBackupRoundsByWriteCycles,
        "MaxBackupRoundsByAccumulated": maxBackupRoundsByAccumulated,
        "MaxBackupRounds": maxBackupRounds,
        "BlockDetails": blockDetails,
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
  - BankSize: Bank size in bytes (default: 32768 = 32KB)
  - NumOfBanks: Number of banks (2 or 4, default: 2)

Example:
  python FeeLifeCycle.py app/app/config/NvM/NvM.json -v
  python FeeLifeCycle.py app/app/config/NvM/NvM.json --BlockSize "64*1024" --PageSize 8
  python FeeLifeCycle.py app/app/config/NvM/NvM.json --NumOfBanks 4 -v
        """,
    )
    parser.add_argument("config", help="Path to NvM.json configuration file")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    parser.add_argument("--BlockSize", help="bank size", default="32*1024", type=str, required=False)
    parser.add_argument("--PageSize", help="page size", default=8, type=int, required=False)
    parser.add_argument("--NumOfBanks", help="number of banks", default=2, type=int, required=False)
    args = parser.parse_args()
    __args = args
    # Evaluate BlockSize expression if provided (supports "32*1024" style)
    if args.BlockSize is not None:
        # Safely parse numeric expressions (only digits, spaces, *, +, -, /, (, ))
        if not all(c in "0123456789*+-/() " for c in args.BlockSize):
            print(f"Error: Invalid BlockSize expression: {args.BlockSize}", file=sys.stderr)
            sys.exit(1)
        args.BlockSize = eval(args.BlockSize, {"__builtins__": {}}, {})

    if not os.path.exists(args.config):
        print(f"Error: Configuration file not found: {args.config}", file=sys.stderr)
        sys.exit(1)

    result = calculateBackupRounds(args.config, args.verbose, args.BlockSize, args.PageSize, args.NumOfBanks)

    if result:
        max_backup_rounds = result["MaxBackupRounds"]
        print("-" * 70)
        print(f"MAXIMUM BACKUP ROUNDS (Worst Case): {max_backup_rounds:,}")
        print("=" * 70)


if __name__ == "__main__":
    main()
