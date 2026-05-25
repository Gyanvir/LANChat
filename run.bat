@echo off
:: This launcher script adds the Qt binaries folder to the session path and runs LANChat.exe
set "PATH=C:\Users\Dell\miniconda3\Library\bin;%PATH%"
start "" "%~dp0release\LANChat.exe"
