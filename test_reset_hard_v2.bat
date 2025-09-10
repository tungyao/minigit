@echo off
echo Testing reset --hard functionality (correct scenario)...

:: 创建测试目录
if exist test_reset_hard_v2 rmdir /s /q test_reset_hard_v2
mkdir test_reset_hard_v2
cd test_reset_hard_v2

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create and commit some files ===
echo Content of file1 > file1.txt
echo Content of file2 > file2.txt
echo Content of file3 > file3.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit with 3 files"

echo.
echo === Step 2: Modify files and commit ===
echo Modified content for file1 > file1.txt
echo Modified content for file2 > file2.txt
echo Modified content for file3 > file3.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Modified all files"

echo.
echo === Step 3: Get current HEAD commit ID ===
for /f "tokens=2" %%i in ('..\build\bin\Release\minigit.exe log ^| findstr "commit" ^| findstr /n "1:"') do set HEAD_COMMIT=%%i
echo Current HEAD commit: %HEAD_COMMIT%

echo.
echo === Step 4: Delete files manually from working directory ===
del file1.txt
del file2.txt
del file3.txt
echo Status after manual deletion (should show all files as deleted):
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Reset --hard to HEAD (should restore all deleted files) ===
..\build\bin\Release\minigit.exe reset --hard
echo.
echo Files after reset --hard:
dir /b *.txt

echo.
echo === Step 6: Verify file contents are from latest commit ===
echo Contents of file1.txt:
type file1.txt
echo.
echo Contents of file2.txt:
type file2.txt
echo.
echo Contents of file3.txt:
type file3.txt

echo.
echo === Step 7: Test reset to earlier commit ===
for /f "tokens=2" %%i in ('..\build\bin\Release\minigit.exe log ^| findstr "commit" ^| findstr /n "2:"') do set FIRST_COMMIT=%%i
echo Resetting to first commit: %FIRST_COMMIT%
..\build\bin\Release\minigit.exe reset --hard %FIRST_COMMIT%

echo.
echo Files after reset to first commit:
dir /b *.txt
echo.
echo Contents should be original:
type file1.txt

echo.
echo Final status:
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_reset_hard_v2

echo.
echo Test completed.