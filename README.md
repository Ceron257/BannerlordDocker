This repository contains everything that's necessary to run Mount and Blade 2 Bannerlord inside a windows docker container.

## Requirements to build the container image

* `winsdksetup.exe` can be downloaded [here](https://go.microsoft.com/fwlink/p/?linkid=2196241).
* `VC_redist.x64.exe` can be downloaded [here](https://aka.ms/vs/17/release/vc_redist.x64.exe).

Place the files above next to `Dockerfile` and build the image. For example you could run:

```
docker build -t bannerlord2image .
```

## Requirements to run Bannerlord in the container

The game recommends 8GB of RAM so make sure to create the container with `--memory 8GB` (you can use more RAM if you like).

You will have to either install the game in the container using SteamCMD, or you can mount the installed game from the host.

The game has to initialize Steam API otherwise it won't start. It's difficult to notice this in the container, as the process will try to show a message box which you can't see because there's no GUI for the container. The easiest way I found was to mount the folder where steam is installed on my host (have your crendentials saved) into the container. You can then start steam.exe in the container and steam will automatically login. On your host you might see a message telling your something is ready to stream. That's the sign that steam started and logged in successfully in the container.

If you want to start the game without a GPU (this is needed for example on Windows 10 as there's no GPU pass-through possible) you'll have to detour the DXGI API to enable the game to start without a graphics card. See [DXGIdetour](DXGIdetour) for instructions to build `dxgidetour.dll`.

To start the game without a GPU you'll need `withdll.exe` from [detours](https://github.com/microsoft/detours) and `dxgidetour.dll`.

You can then start the game by running the following command in the container:
```
<path-to-withdll.exe>\withdll.exe -d:<path-to-dxgidetour>\dxgidetour.dll .\Bannerlord.exe /singleplayer _MODULES_*Native*SandBoxCore*CustomBattle*SandBox*StoryMode*_MODULES_
```
Please make sure to replace `<path-to-withdll.exe>` and `<path-to-dxgidetour>` with meaningful values. The command should be executed in `bin\Win64_Shipping_Client` in the directory where Bannerlord is installed.

## Tips for debugging

In the container there's `cdb.exe` available under `C:\BuildTools\Debuggers\x64\cdb.exe`. You can use it to start Bannerlord with an attached debugger if something is strange by running `cdb.exe Bannerlord.exe <args for bannerlord>`.

When using `withdll.exe` you should run `cdb` with the `-o` flag to enable debugging of child processes. You can use the [|](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/-s--set-current-process-) command to switch to the game's process.

Useful commands in cdb:

* [k](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/k--kb--kc--kd--kp--kp--kv--display-stack-backtrace-) to display stack traces
* [~](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/---thread-status-) to see and switch threads
  * e.g. `~* k` to show stacks for all threads
* there's also the [SOS Debugger extension](https://docs.microsoft.com/en-us/dotnet/framework/tools/sos-dll-sos-debugging-extension) which can be useful to inspect C# stack traces
  * load it by executing `.loadby sos clr` the
    * `!clrstack` to show the clr stack for the current thread (if there is any)
    * `!eestack` to run `!clrstack` for all current threads (useful to get an overview)