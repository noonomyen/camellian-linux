#!/usr/bin/env python3
import os
import subprocess
import sys
import hashlib

# Paths
WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INPUT_IMG = os.path.join(WORKSPACE, "images", "tee.stock.img")
TEMP_IMG = os.path.join(WORKSPACE, "out", "tee_patched_temp.img")
OUTPUT_IMG = os.path.join(WORKSPACE, "out", "tee_el2.img")
SIGN_TOOL = os.path.join(WORKSPACE, "pwnage24mtk", "sign_mtk_cert.py")
VERIFY_TOOL = os.path.join(WORKSPACE, "pwnage24mtk", "verify_mtk_image.py")

# Expected SHA256 of the correct MT6833 tee.stock.img
EXPECTED_HASH = "69c9d2839be47a359c524b71f492228b798fc7702ecbeefc53ce842ca8a367a8"

def log(msg):
    print(f"[patch-tee] {msg}")

def log_error(msg):
    print(f"[error] {msg}")

def get_file_hash(filepath):
    sha256 = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha256.update(chunk)
    return sha256.hexdigest()

def main():
    # 1. Validate required files exist
    required_files = [INPUT_IMG, SIGN_TOOL, VERIFY_TOOL]
    for req_file in required_files:
        if not os.path.exists(req_file):
            log_error(f"Required file not found: {req_file}")
            sys.exit(1)

    log(f"Input:  {INPUT_IMG}")
    
    # 2. Hash verification
    actual_hash = get_file_hash(INPUT_IMG)
    if actual_hash != EXPECTED_HASH:
        log_error(f"SHA256 mismatch!")
        log_error(f"Expected : {EXPECTED_HASH}")
        log_error(f"Got      : {actual_hash}")
        log_error("The patches might be applied at the wrong offsets. Aborting.")
        sys.exit(1)
    
    log(f"SHA256 verified: {actual_hash}")

    # 3. Read and Patch Data
    with open(INPUT_IMG, "rb") as f:
        data = bytearray(f.read())

    gz_patch_offset = 0x45D8
    gz_patch_bytes = b'\x00\x00\x80\x52\xc0\x03\x5f\xd6'
    
    bl33_patch_offset = 0xBF2C
    bl33_patch_bytes = b'\x00\x04\xa9\xd2'

    # Boundary check
    if len(data) < bl33_patch_offset + len(bl33_patch_bytes):
        log_error("Input image is too small to contain the expected offsets.")
        sys.exit(1)

    log(f"Applying GZ bypass patch at offset 0x{gz_patch_offset:X}")
    data[gz_patch_offset : gz_patch_offset + len(gz_patch_bytes)] = gz_patch_bytes

    log(f"Applying BL33 redirect patch (to LK) at offset 0x{bl33_patch_offset:X}")
    data[bl33_patch_offset : bl33_patch_offset + len(bl33_patch_bytes)] = bl33_patch_bytes

    # 4. Save temporary patched file
    os.makedirs(os.path.dirname(TEMP_IMG), exist_ok=True)
    with open(TEMP_IMG, "wb") as f:
        f.write(data)
    
    # 5. Sign and Verify
    try:
        log("Signing the patched image...")
        sign_cmd = ["python3", SIGN_TOOL, "-w", "-o", OUTPUT_IMG, TEMP_IMG]
        result = subprocess.run(sign_cmd)
        
        if result.returncode != 0:
            log_error(f"Signing tool failed with code {result.returncode}")
            sys.exit(1)
            
        log("\nVerifying the signed image...")
        verify_cmd = ["python3", VERIFY_TOOL, OUTPUT_IMG]
        verify_result = subprocess.run(verify_cmd)
        
        if verify_result.returncode != 0:
            log_error(f"Verification tool failed with code {verify_result.returncode}")
            sys.exit(1)
            
        log(f"\nOutput: {OUTPUT_IMG}")
        log("Status: Success")
        
    finally:
        # 6. Cleanup temp file unconditionally
        if os.path.exists(TEMP_IMG):
            os.remove(TEMP_IMG)

if __name__ == "__main__":
    main()
