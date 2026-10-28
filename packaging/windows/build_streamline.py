#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the optional Windows x64 bridge with pinned production NVIDIA libraries."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile

VERSION = "2.14.16"
SHA256 = "32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b"
URL = f"https://github.com/NVIDIA-RTX/Streamline/releases/download/v{VERSION}/streamline-sdk-v{VERSION}.zip"
DLLS = ("sl.interposer.dll", "sl.common.dll", "sl.dlss_g.dll", "sl.reflex.dll",
        "sl.pcl.dll", "nvngx_dlssg.dll", "NvLowLatencyVk.dll")


def checked_members(archive, destination):
    destination = destination.resolve()
    for member in archive.infolist():
        target = (destination / member.filename).resolve()
        if not target.is_relative_to(destination):
            raise ValueError(f"SDK archive path escapes staging directory: {member.filename}")
    return archive.infolist()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="Previously downloaded official SDK ZIP")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = root / "out"
    out.mkdir(exist_ok=True)
    archive = args.archive or out / f"streamline-sdk-v{VERSION}.zip"
    if not archive.exists():
        with urllib.request.urlopen(URL, timeout=120) as response, archive.open("wb") as target:
            shutil.copyfileobj(response, target)
    with archive.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    if digest != SHA256:
        raise ValueError("Streamline SDK SHA-256 mismatch, refusing to build or copy DLLs")
    sdk = out / f"streamline-sdk-{VERSION}"
    with zipfile.ZipFile(archive) as source:
        source.extractall(sdk, members=checked_members(source, sdk))
    # Only Vulkan headers are exposed to MSVC, never MSYS2's C headers.
    headers = out / "streamline-vkinclude"
    for folder in ("vulkan", "vk_video"):
        shutil.copytree(Path("C:/msys64/clang64/include") / folder,
                        headers / folder, dirs_exist_ok=True)
    cmake = shutil.which("cmake") or "C:/msys64/clang64/bin/cmake.exe"
    build = out / "streamline-bridge"
    subprocess.run([cmake, "-S", str(root / "gpu/streamline_bridge"), "-B", str(build),
                    "-G", "Visual Studio 17 2022", "-A", "x64",
                    f"-DSTREAMLINE_SDK_ROOT={sdk}", f"-DVULKAN_INCLUDE={headers}"], check=True)
    subprocess.run([cmake, "--build", str(build), "--config", "Release", "--target",
                    "spartanport_streamline", "streamline-bridge-test"], check=True)
    subprocess.run([str(build / "Release/streamline-bridge-test.exe")], check=True, timeout=30,
                   creationflags=subprocess.CREATE_NO_WINDOW)
    stage = out / "streamline"
    stage.mkdir(exist_ok=True)
    shutil.copy2(build / "Release/spartanport_streamline.dll", stage)
    for name in DLLS:
        shutil.copy2(sdk / "bin/x64" / name, stage)
    licenses = stage / "LICENSES"
    licenses.mkdir(exist_ok=True)
    shutil.copy2(sdk / "license.txt", licenses / "NVIDIA-Streamline.txt")
    shutil.copy2(root / "gpu/streamline_bridge/LICENSE.txt", licenses / "spartanport_streamline-MIT.txt")
    shutil.copy2(sdk / "3rd-party-licenses.md", licenses / "NVIDIA-Streamline-third-party.txt")
    for name in ("reflex.license.txt", "nvngx_dlss.license.txt"):
        shutil.copy2(sdk / "bin/x64" / name, licenses / name)
    print(f"Streamline {VERSION} production package ready: {stage}")


if __name__ == "__main__":
    main()
