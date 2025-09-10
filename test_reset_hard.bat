@echo off
echo Testing reset --hard functionality...

:: 创建测试目录
if exist test_reset_hard rmdir /s /q test_reset_hard
mkdir test_reset_hard
cd test_reset_hard

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
echo === Step 2: Create another commit ===
echo More content >> file2.txt
echo Content of file4 > file4.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Second commit - updated file2, added file4"

echo.
echo === Step 3: Get current commit ID and show status ===
..\build\bin\Release\minigit.exe log
echo.
echo Current status:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 4: Delete some files manually from working directory ===
del file1.txt
del file4.txt
echo Modified file3 content >> file3.txt
echo Status after manual deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Reset --hard to HEAD (should restore deleted files) ===
..\build\bin\Release\minigit.exe reset --hard
echo.
echo Files after reset --hard:
dir /b *.txt

echo.
echo === Step 6: Verify file contents ===
if exist file1.txt (
    echo file1.txt exists - GOOD
    type file1.txt
) else (
    echo file1.txt missing - BAD
)

if exist file4.txt (
    echo file4.txt exists - GOOD  
    type file4.txt
) else (
    echo file4.txt missing - BAD
)

echo.
echo Final status:
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_reset_hard

echo.
echo Test completed.