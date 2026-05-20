# Quick Setup Checklist for 3D Telecommunications

## Prerequisites Checklist
- [ ] Windows 11 x64 with admin access
- [ ] ~40GB free disk space (for VS, CUDA, OpenCV, vcpkg)
- [ ] Decent internet connection (large downloads)
- [ ] NVIDIA GPU RTX 2080 or better

## Installation Order

### Phase 1: Visual Studio (45 min)
- [ ] Download Visual Studio Community from https://visualstudio.microsoft.com/downloads/
- [ ] Run installer
- [ ] Select "Desktop development with C++"
- [ ] Wait for installation
- [ ] Verify: Open new PowerShell, run `cl.exe /?`

### Phase 2: CUDA 11.7 (20 min)
- [ ] Download from https://developer.nvidia.com/cuda-11-7-1-download-archive
- [ ] Select Windows 11 x64
- [ ] Run installer
- [ ] Use defaults
- [ ] Verify: New PowerShell, run `nvcc --version`

### Phase 3: vcpkg Dependencies (60 min) ⚠️ REQUIRES Phase 1
- [ ] Open PowerShell as Administrator
- [ ] Run: `C:\vcpkg\3DTelecommunications\install-dependencies.ps1`
- [ ] Wait for completion

### Phase 4: OpenCV with CUDA (90 min) ⚠️ REQUIRES Phase 1 & 2
See detailed instructions in: `SETUP_WINDOWS.md` → Step 3

```powershell
# Quick commands:
cd C:\vcpkg\3DTelecommunications\Dependencies
git clone --depth 1 --branch 4.5.4 https://github.com/opencv/opencv.git
cd opencv
git clone --depth 1 https://github.com/opencv/opencv_contrib.git modules/opencv_contrib
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DWITH_CUDA=ON
msbuild OpenCV.sln /p:Configuration=Release /p:Platform=x64
msbuild INSTALL.vcxproj /p:Configuration=Release /p:Platform=x64
```

### Phase 5: Configure & Build (5 min)
- [ ] Edit: `PeabodyConfigurationMacros.props`
- [ ] Set: `<Peabody_Dependency_Dir>C:\vcpkg\installed\x64-windows</Peabody_Dependency_Dir>`
- [ ] Open: `DeformableFusion\DeformableFusion.sln`
- [ ] Set config: Release + x64
- [ ] Build → Build Solution

## File References
- `SETUP_WINDOWS.md` - Detailed step-by-step guide
- `install-dependencies.ps1` - Automated vcpkg installer
- `README.md` - Project overview and architecture

## Troubleshooting Links
- Visual Studio: https://docs.microsoft.com/en-us/visualstudio/install/install-visual-studio
- CUDA: https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/
- OpenCV CUDA: https://docs.opencv.org/4.5.4/d1/d5c/group__cuda.html
- vcpkg: https://github.com/Microsoft/vcpkg

## Total Time Estimate
- **Total: ~4-5 hours** (mostly waiting for installations)
- Can parallelize some steps if desired

---
**Started**: May 19, 2026
**Status**: Prerequisites & guides created, ready for installation
