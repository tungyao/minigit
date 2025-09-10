@echo off
echo Testing file deletion detection including staged-then-deleted files...

:: 创建测试目录
if exist test_deletion rmdir /s /q test_deletion
mkdir test_deletion
cd test_deletion

:: 初始化仓库
..\build\bin\Release\minigit.exe init

:: 测试场景1: 普通的已提交文件删除
echo This is committed file > committed_file.txt
..\build\bin\Release\minigit.exe add committed_file.txt
..\build\bin\Release\minigit.exe commit "Add committed file"

echo.
echo === Scenario 1: Delete committed file ===
del committed_file.txt
echo Status after deleting committed file:
..\build\bin\Release\minigit.exe status

echo.
echo === Scenario 2: Stage new file then delete it ===
:: 测试场景2: 您提到的关键场景 - 暂存新文件后删除
echo This is a staged file > staged_file.txt
..\build\bin\Release\minigit.exe add staged_file.txt
echo Status after staging new file:
..\build\bin\Release\minigit.exe status

echo.
echo Now deleting the staged file from working directory...
del staged_file.txt
echo Status after deleting staged file (this should show AD status):
..\build\bin\Release\minigit.exe status

echo.
echo === Scenario 3: Stage modification then delete ===
:: 测试场景3: 暂存修改后删除
echo Modified content > committed_file.txt
..\build\bin\Release\minigit.exe add committed_file.txt
echo Status after staging modification:
..\build\bin\Release\minigit.exe status

echo.
echo Now deleting the modified file...
del committed_file.txt
echo Status after deleting modified file (this should show MD status):
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_deletion

echo.
echo Test completed.