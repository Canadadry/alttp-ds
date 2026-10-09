@echo off
setlocal
set DEVKITPRO=C:/devkitPro
set DEVKITARM=C:/devkitPro/devkitARM
set PATH=C:\devkitPro\msys2\usr\bin;C:\devkitPro\devkitARM\bin;%PATH%

if exist "%~dp0zelda3_assets.dat" (
  copy /Y "%~dp0zelda3_assets.dat" "%~dp0platform\nds\nitrofiles\zelda3_assets.dat" >nul
) else if exist "%USERPROFILE%\Desktop\ALTTP-DS\zelda3_assets.dat" (
  copy /Y "%USERPROFILE%\Desktop\ALTTP-DS\zelda3_assets.dat" "%~dp0platform\nds\nitrofiles\zelda3_assets.dat" >nul
) else if exist "%~dp0data\zelda3_assets.dat" (
  copy /Y "%~dp0data\zelda3_assets.dat" "%~dp0platform\nds\nitrofiles\zelda3_assets.dat" >nul
)

make %*
