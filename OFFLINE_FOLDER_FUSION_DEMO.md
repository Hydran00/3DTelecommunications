# OfflineFolderFusion demo

This trimmed tree keeps only the projects needed to build and run the
OfflineFolderFusion demo.

## Requirements

- Windows + Visual Studio 2019 C++ Build Tools
- CUDA 11.7, with `CUDA_PATH_V11_7` defined
- vcpkg dependencies installed under `C:\vcpkg\installed\x64-windows`
- Dataset folder, for example:
  `C:\vcpkg\VolumeDeform\upperbody\data`

The project property sheets resolve dependencies through
`PeabodyConfigurationMacros.props`. On this workstation they use
`C:\vcpkg\installed\x64-windows`.

## Build

From this repository root:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  .\DeformableFusion\OfflineFolderFusion\OfflineFolderFusion.vcxproj `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /m:8
```

The executable is generated here:

```text
DeformableFusion\x64\Release\OfflineFolderFusion.exe
```

## Run Upperbody Demo

```powershell
.\DeformableFusion\x64\Release\OfflineFolderFusion.exe `
  --input C:\vcpkg\VolumeDeform\upperbody\data `
  --depth-token depth `
  --rgb-token color `
  --output C:\vcpkg\VolumeDeform\upperbody\fusion_out_fg `
  --camera-count 1 `
  --gpu 0 `
  --config C:\vcpkg\3DTelecommunications\ConfigFileExamples\OfflineFolderFusion_upperbody.cfg `
  --fx 570.342 `
  --fy 570.342 `
  --cx 320.0 `
  --cy 240.0 `
  --bbox -150,150,-150,150,30,300 `
  --viewer3d `
  --dump-surface-every 10 `
  --dump-surface-format ply
```

Viewer controls:

- Left mouse: rotate
- Right mouse: pan
- Wheel: zoom
- `G`: toggle embedded graph
- `R`: reset view
- `Esc` / `Q`: close viewer

## Kept Projects

- `DeformableFusion/OfflineFolderFusion`
- `DeformableFusion/FastNonrigidMatching`
- `DeformableFusion/NonRigidMatch`
- `DeformableFusion/Basics`
- `DeformableFusion/CameraView`
- `DeformableFusion/CSurface`
- `DeformableFusion/Utility`
- `DeformableFusion/Common`
- `DeformableFusion/FusionDemo-MultiView`
- `McLib2/LibUtility`

`FusionDemo-MultiView` is kept because `OfflineFolderFusion.vcxproj`
compiles `FusionConfig.cpp` and `GlobalDataStatic.cpp` from there.
