@echo off
echo Testing reset --hard with historical files and complex scenarios...

:: 创建测试目录
if exist test_reset_complex rmdir /s /q test_reset_complex
mkdir test_reset_complex
cd test_reset_complex

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create initial commit ===
echo Content A > fileA.txt
echo Content B > fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit: A, B"

echo.
echo === Step 2: Add more files and commit ===
echo Content C > fileC.txt
echo Updated B > fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Second commit: add C, update B"

echo.
echo === Step 3: Remove fileA and commit ===
del fileA.txt
..\build\bin\Release\minigit.exe add fileA.txt
..\build\bin\Release\minigit.exe commit -m "Third commit: remove A"

echo.
echo === Step 4: Show current status ===
echo Current commit log:
..\build\bin\Release\minigit.exe log --oneline
echo.
echo Current status:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Delete files from working directory ===
del fileB.txt
del fileC.txt
echo Status after manual deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 6: Reset --hard to second commit (should restore A, B, C) ===
for /f "tokens=1" %%i in ('..\build\bin\Release\minigit.exe log --oneline ^| findstr "Second commit"') do set SECOND_COMMIT=%%i
echo Resetting to second commit: %SECOND_COMMIT%
..\build\bin\Release\minigit.exe reset --hard %SECOND_COMMIT%

echo.
echo === Step 7: Verify restoration ===
echo Files after reset:
dir /b *.txt
echo.
echo File contents:
echo fileA.txt: && type fileA.txt
echo fileB.txt: && type fileB.txt
echo fileC.txt: && type fileC.txt
echo.
echo Final status:
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_reset_complex

echo.
echo Complex reset test completed!