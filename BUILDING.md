# Building the FEASR source

## Source target

The FEASR PC1.2 source target is based on upstream commit `989f215e81f3595dc4526a1311437946646a998a` (Calamity 2.2.8), followed by the source commits listed in [SOURCE-MANIFEST.json](SOURCE-MANIFEST.json). Do not update the upstream baseline when reproducing this target.

The native project uses CMake 3.21 or newer, C++23, CommonLibSSE-NG, and nlohmann/json. The vcpkg manifest pins its registry baselines; retain `skse/CalamityAffixes/vcpkg.json` and `vcpkg-configuration.json` when configuring dependencies. Use a Windows x64 target and a CommonLibSSE-NG flatrim package compatible with the Skyrim runtime you intend to support.

## Configure and compile

The checked-in `linux-release-commonlibsse` preset expects a project-local CommonLib install at `skse/CalamityAffixes/extern/CommonLibSSE-NG/install.linux-clangcl-rel`. That build/install directory is intentionally not committed. If you have that compatible install, the preset entry points are:

```sh
cmake --preset linux-release-commonlibsse
cmake --build --preset linux-release-commonlibsse --target CalamityAffixes
```

For another toolchain, configure with the vcpkg toolchain and point CMake at the compatible CommonLibSSE package. Example PowerShell command, with `VCPKG_ROOT` set to your vcpkg checkout and `COMMONLIB_PREFIX` set to its CommonLibSSE-NG install:

```powershell
cmake -S skse/CalamityAffixes -B build/feasr-pc12 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="$env:COMMONLIB_PREFIX"
cmake --build build/feasr-pc12 --config Release --target CalamityAffixes
```

The accepted PC1.2 artifact was produced in the project's WSL `clang-cl`/`lld-link` lane. It used clang-cl 19, an MSVC 14.44 / Windows SDK 10.0.26100 sysroot, CommonLibSSE-NG, and the PC1.1 build's object set. Its exact DLL checksum and source hashes are recorded in [SOURCE-MANIFEST.json](SOURCE-MANIFEST.json).

## Reproducibility boundary

An independent clean build of the public candidate completed on 2026-11-13 from an empty plugin build directory and a clean CommonLibSSE-NG build. It used Clang/clang-cl 19.1.7, lld-link, MSVC 14.44.35207, Windows SDK 10.0.26100.0, and CommonLibSSE-NG commit `b6e0c134b08a2108300c8d033c3b81860ce62354`. The output SHA-256 was `03ea6d92e41a984a156cbd91307e9383d1745bb4c5d5d0707a9b395290915f6c` (3,960,832 bytes).

This output does **not** match the accepted PC1.2 DLL byte-for-byte: the accepted artifact is `b987160bc406bef8dd773db6ac6b59684d311bcea9e5b9f06f847fc4d7a32dd4` (3,963,392 bytes). PE inspection shows differences in `.text` size/content and downstream section RVAs/sizes, not just the PE timestamp. The accepted build reused most objects from the earlier PC1.1 object set, while this build recompiled every plugin translation unit and rebuilt CommonLibSSE-NG. The exact object-level cause of each changed code byte has not been isolated. Treat the clean build as proof that this source candidate compiles and links, not as binary equivalence or gameplay certification.

No test suite or gameplay certification was run as part of this source audit. Before a binary release, complete the source and packaging gates and separately complete gameplay certification. Do not publish build directories, caches, object files, PDBs, or local packages.
