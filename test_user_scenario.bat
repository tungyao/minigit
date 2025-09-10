@echo off
echo Testing reset --hard with user's specific scenario...

:: 创建测试目录
if exist test_user_scenario rmdir /s /q test_user_scenario
mkdir test_user_scenario
cd test_user_scenario

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Reproduce user's scenario ===
echo === Step 1: Create files and commit ===
echo Content A > fileA.txt
echo Content B > fileB.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Commit 1: added A, B, C"

echo.
echo === Step 2: Modify one file and commit ===
echo Modified B >> fileB.txt
..\build\bin\Release\minigit.exe add fileB.txt
..\build\bin\Release\minigit.exe commit -m "Commit 2: modified B"

echo.
echo === Step 3: Get latest commit ID ===
for /f "tokens=2" %%i in ('..\build\bin\Release\minigit.exe log ^| findstr "commit" ^| findstr /n "1:"') do set LATEST_COMMIT=%%i
echo Latest commit ID: %LATEST_COMMIT%

echo.
echo === Step 4: Manually delete files (simulating user's issue) ===
del fileA.txt
del fileC.txt
echo.
echo Status after deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Execute reset --hard to latest commit ===
echo This should restore the deleted files...
..\build\bin\Release\minigit.exe reset --hard %LATEST_COMMIT%

echo.
echo === Step 6: Check if files are restored ===
echo Files after reset --hard:
dir /b *.txt

echo.
echo Verification:
if exist fileA.txt (
    echo ✓ fileA.txt restored successfully
    echo Content: 
    type fileA.txt
) else (
    echo ✗ fileA.txt NOT restored - this is the bug
)

if exist fileB.txt (
    echo ✓ fileB.txt exists
    echo Content: 
    type fileB.txt
) else (
    echo ✗ fileB.txt missing
)

if exist fileC.txt (
    echo ✓ fileC.txt restored successfully
    echo Content: 
    type fileC.txt
) else (
    echo ✗ fileC.txt NOT restored - this is the bug
)

echo.
echo Final status:
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_user_scenario

echo.
echo Test completed.
echo If fileA.txt and fileC.txt were restored, the bug is fixed!