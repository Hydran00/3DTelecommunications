# 3D Telecommunications - Windows 11 Setup Guide

## Prerequisites
You need Windows 11 x64 with administrator access.

## Installation Steps

### Step 1: Install Visual Studio 2022 Build Tools
**This is CRITICAL and must be done first.**

1. Download Visual Studio Community (free):
   - Visit: https://visualstudio.microsoft.com/downloads/
   - Choose "Community" edition

2. Run the installer and select **"Desktop development with C++"**
   - This installs MSVC compiler, Windows SDK, and CMake tools
   - Installation takes ~30-45 minutes

3. After installation, verify by opening PowerShell and running:
   ```powershell
   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
   cl.exe /?
   ```
   You should see compiler version info.

### Step 2: Install CUDA 11.7 Toolkit from NVIDIA

1. Download CUDA 11.7.1:
   - Go to: https://developer.nvidia.com/cuda-11-7-1-download-archive
   - Select: Windows → x86_64 → 11 (Windows 10/11)
   - Download the installer (~3GB)

2. Run the installer:
   - Accept defaults
   - Include cuDNN library if prompted
   - Installation takes ~15-20 minutes

3. Verify installation (new PowerShell window):
   ```powershell
   nvcc --version
   ```
   Should show: `cuda_11.7.r11.7`

### Step 3: Install OpenCV 4.5.4 with CUDA Support

OpenCV needs to be built with CUDA support. This is a separate process:

1. Download OpenCV source:
   ```powershell
   cd C:\vcpkg\3DTelecommunications\Dependencies
   git clone --depth 1 --branch 4.5.4 https://github.com/opencv/opencv.git
   cd opencv
   git clone --depth 1 https://github.com/opencv/opencv_contrib.git modules/opencv_contrib
   ```

2. Build with CMake:
   ```powershell
   mkdir build
   cd build
    cmake .. `
       -G "Visual Studio 17 2022" `
     -A x64 `
     -DCMAKE_BUILD_TYPE=Release `
     -DWITH_CUDA=ON `
     -DCUDA_TOOLKIT_ROOT_DIR="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.7" `
     -DCUDA_ARCH_BIN="7.5" `
     -DBUILD_opencv_python3=OFF `
     -DOPENCV_EXTRA_MODULES_PATH=../modules/opencv_contrib/modules
   
   msbuild OpenCV.sln /p:Configuration=Release /p:Platform=x64
   msbuild INSTALL.vcxproj /p:Configuration=Release /p:Platform=x64
   ```
   This takes 1-2 hours to build.

### Step 4: Install Core Dependencies via vcpkg

Once Visual Studio is installed and verified, run these commands in PowerShell:

```powershell
# First batch of core libraries
C:\vcpkg\vcpkg.exe install zlib boost zeromq jsoncpp lz4 glew freeglut xerces-c qhull cppzmq --triplet x64-windows

# Additional required libraries
C:\vcpkg\vcpkg.exe install ceres-solver vxl --triplet x64-windows
```

Each installation takes 10-30 minutes depending on your system.

### Step 5: Configure the Project

After all dependencies are installed:

1. Open: `c:\vcpkg\3DTelecommunications\PeabodyConfigurationMacros.props`

2. Update `Peabody_Dependency_Dir` to point to your vcpkg installation:
   ```xml
   <Peabody_Dependency_Dir>C:\vcpkg\installed\x64-windows</Peabody_Dependency_Dir>
   ```

3. Open `DeformableFusion\DeformableFusion.sln` in Visual Studio 2022

4. Set configuration to **Release x64**

5. Build → Build Solution

## Summary of Dependencies

### From vcpkg (Automatic):
- ✓ boost 1.91.0 (newer version, compatible)
- ✓ zlib 1.3.2
- ✓ jsoncpp 1.9.6
- ✓ lz4 1.10.0
- ✓ glew 2.3.1
- ✓ freeglut 3.8.0
- ✓ xerces-c 3.3.0
- ✓ qhull 8.0.2
- ✓ cppzmq 4.8.1
- ✓ zeromq 4.3.5
- ✓ ceres-solver (latest)
- ✓ vxl 2.0.2

### Manual Installs:
- Windows 11 x64
- Visual Studio 2022 with C++ tools
- CUDA 11.7 Toolkit
- OpenCV 4.5.4 (built with CUDA)

## Troubleshooting

**Issue: "cl.exe not found"**
- Visual Studio C++ tools are not installed
- Re-run Visual Studio installer and select "Desktop development with C++"

**Issue: "nvcc not found"**
- CUDA toolkit not installed or not in PATH
- Re-install CUDA 11.7 from NVIDIA

**Issue: vcpkg packages fail to compile**
- Ensure Visual Studio is fully installed with C++ workload
- Run: `C:\vcpkg\vcpkg.exe upgrade --no-dry-run`

**Issue: OpenCV build fails**
- Check CUDA toolkit path is correct
- Verify GPU compute capability (check CUDA_ARCH_BIN for your GPU)
- See: https://developer.nvidia.com/cuda-gpus

## Estimated Total Time
- Visual Studio: 45 minutes
- CUDA: 20 minutes
- OpenCV with CUDA: 90 minutes
- vcpkg dependencies: 60 minutes
- **Total: ~4 hours**

## Next Steps
After setup, see `README.md` for building the Fusion system.
