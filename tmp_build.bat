@echo off
call "O:\Programs\VisualStudio\26\Common7\Tools\LaunchDevCmd.bat" /v:off
cmake --build "O:\__PROJECT\Own\EugeneSystems\WargameModManager\build\windows-release" --config Release --parallel 4
