@echo off
setlocal
set DEVKITPRO=C:/devkitPro
set DEVKITARM=C:/devkitPro/devkitARM
set PATH=C:\devkitPro\msys2\usr\bin;C:\devkitPro\devkitARM\bin;%PATH%

make %*
set ERR=%ERRORLEVEL%
if %ERR%==0 (
  echo.
  echo Built %~dp0lttp-ds.nds
  goto done
)
echo.
echo Build failed. lttp-ds.nds was not created.
echo Put your USA ROM at vendor\zelda3\assets\zelda3.sfc.
:done
echo %cmdcmdline% | find /i "%~f0" >nul
if not errorlevel 1 pause
exit /b %ERR%
