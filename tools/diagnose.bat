@echo off
setlocal
echo == SirioC quick diagnose ==
where SirioC.exe
echo -- Running UCI --
SirioC.exe uci || echo ExitCode=%errorlevel%
echo -- Done --
pause
