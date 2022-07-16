FROM mcr.microsoft.com/windows:21H1

COPY winsdksetup.exe /temp/winsdksetup.exe
COPY VC_redist.x64.exe /temp/VCredist.exe

WORKDIR C:\\temp

RUN powershell -Command Start-Process C:\\temp\\VCredist.exe -Wait -ArgumentList '/install /quiet /norestart'

#Install cdb for debugging
RUN powershell -Command Start-Process C:\\temp\\winsdksetup.exe -Wait -ArgumentList '/features OptionId.WindowsDesktopDebuggers /installpath C:\BuildTools /norestart /q /log C:\BuildTools\install-log.txt'

CMD [ "cmd" ]