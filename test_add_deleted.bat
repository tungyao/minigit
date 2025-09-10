@echo off
echo Testing add command with deleted files...

:: 创建测试目录
if exist test_add_deleted rmdir /s /q test_add_deleted
mkdir test_add_deleted
cd test_add_deleted

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create and commit some files ===
echo Content A > fileA.txt
echo Content B > fileB.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit with A, B, C"

echo.
echo === Step 2: Check initial status ===
..\build\bin\Release\minigit.exe status

echo.
echo === Step 3: Delete some files manually ===
del fileA.txt
del fileC.txt
echo Status after manual deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 4: Try to add all changes (including deletions) ===
echo This should detect and stage the deletions...
..\build\bin\Release\minigit.exe add .

echo.
echo === Step 5: Check status after add . ===
echo Status should show staged deletions:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 6: Commit the deletions ===
..\build\bin\Release\minigit.exe commit -m "Deleted fileA and fileC"

echo.
echo === Step 7: Final verification ===
echo Final status (should be clean):
..\build\bin\Release\minigit.exe status

echo.
echo Check commit log:
..\build\bin\Release\minigit.exe log

:: 清理
cd ..
rmdir /s /q test_add_deleted

echo.
echo Test completed.