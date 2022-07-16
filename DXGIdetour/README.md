This folder contains the source code to build `dxgidetour.dll` which can be used to detour Bannerlord's calls to enable the game to run without a graphics card. 

This is done by changing the vendor ID of the adapters descriptor from microsoft's vendor ID to nvidia's vendor ID. That way the game will accept to start with "Microsoft Default Renderer" without requiring a hardware accelerated driver. Keep in mind though, that the game will do all rendering on the CPU!

### Build instructions

  1. Get [detours' source code](https://github.com/microsoft/detours) and run `nmake` in a Visual Studio developer prompt in its root directory. You'll have to have Visual Studio setup to build C++ applications or at least have C++ Build Tools available. 
  2. Generate the project files using CMake and provide the path to detours as `DETOURS_PATH` (e.g. `cmake -D DETOURS_PATH=D:\Detours .`)
  3. Compile the project (e.g. `cmake --build . --config Release`)