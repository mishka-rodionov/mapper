@echo off
set "MAPPER_DIR=H:\orienteering\soft\OpenOrienteering-Mapper-0.9.7-Windows-x64"
set "PATH=%MAPPER_DIR%;%MAPPER_DIR%\plugins\platforms;H:\programm\msys\mingw64\bin;H:\programm\msys\usr\bin;%PATH%"
set "QT_PLUGIN_PATH=%MAPPER_DIR%\plugins"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%MAPPER_DIR%\plugins\platforms"
start "" /D "%MAPPER_DIR%" "%MAPPER_DIR%\Mapper.exe"
