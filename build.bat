@echo off
setlocal
set DEVKITPRO=C:/devkitPro
set DEVKITARM=C:/devkitPro/devkitARM
set PATH=C:\devkitPro\msys2\usr\bin;C:\devkitPro\devkitARM\bin;%PATH%
make %*
