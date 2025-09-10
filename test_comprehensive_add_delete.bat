@echo off
echo Testing comprehensive add and delete functionality...

:: 创建测试目录
if exist test_comprehensive rmdir /s /q test_comprehensive
mkdir test_comprehensive
cd test_comprehensive

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create initial commit ===
echo Content 1 > file1.txt
echo Content 2 > file2.txt
echo Content 3 > file3.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit with 3 files"

echo.
echo === Step 2: Modify and delete files ===
echo Modified content >> file2.txt
del file1.txt
del file3.txt
echo.
echo Status after changes:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 3: Add all changes (including deletions) ===
..\build\bin\Release\minigit.exe add .
echo.
echo Status after add:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 4: Commit changes ===
..\build\bin\Release\minigit.exe commit -m "Modified file2, deleted file1 and file3"

echo.
echo === Step 5: Final status ===
..\build\bin\Release\minigit.exe status

echo.
echo === Step 6: Delete remaining file and test reset --hard ===
del file2.txt
echo Status after deleting file2:
..\build\bin\Release\minigit.exe status

echo.
echo Reset --hard to restore file2:
..\build\bin\Release\minigit.exe reset --hard

echo.
echo Files after reset:
dir /b *.txt

echo.
echo Final status:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 7: Test log ===
..\build\bin\Release\minigit.exe log

:: 清理
cd ..
rmdir /s /q test_comprehensive

echo.
echo Comprehensive test completed successfully!