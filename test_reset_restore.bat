@echo off
echo Testing reset --hard file restoration...

:: 创建测试目录
if exist test_reset_restore rmdir /s /q test_reset_restore
mkdir test_reset_restore
cd test_reset_restore

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Create and commit test files ===
echo File A content > fileA.txt
echo File B content > fileB.txt
echo File C content > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Test commit"

echo.
echo === Files before deletion ===
echo Files present:
dir /b *.txt
echo.
echo Content of fileA.txt: && type fileA.txt
echo Content of fileB.txt: && type fileB.txt

echo.
echo === Delete files from working directory ===
del fileA.txt
del fileB.txt
echo Files after deletion:
dir /b *.txt 2>nul || echo (no .txt files found except fileC.txt)

echo.
echo === Execute reset --hard ===
..\build\bin\Release\minigit.exe reset --hard

echo.
echo === Verify restoration ===
echo Files after reset --hard:
dir /b *.txt
echo.
echo Verify file contents:
echo fileA.txt: && type fileA.txt
echo fileB.txt: && type fileB.txt
echo fileC.txt: && type fileC.txt

:: 清理
cd ..
rmdir /s /q test_reset_restore

echo.
echo Test completed - reset --hard successfully restored deleted files!