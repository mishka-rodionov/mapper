@echo off
set "PATH=H:\programm\msys\mingw64\bin;H:\programm\msys\usr\bin;%PATH%"
set "QT_PLUGIN_PATH=H:\programm\msys\mingw64\share\qt5\plugins"
set "QT_QPA_PLATFORM_PLUGIN_PATH=H:\programm\msys\mingw64\share\qt5\plugins\platforms"
start "" "%~dp0build\mingw64\src\Mapper.exe"
