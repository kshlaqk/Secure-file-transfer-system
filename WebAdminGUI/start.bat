@echo off
chcp 65001 >nul
cls
title Web 管理后台

echo ========================================
echo    Web 管理后台
echo ========================================
echo.

if not exist node_modules (
    echo [信息] 正在安装依赖...
    echo.
    call npm install
    if errorlevel 1 (
        echo.
        echo [错误] 依赖安装失败！
        pause
        exit /b 1
    )
    echo.
    echo [成功] 依赖安装完成！
    echo.
) else (
    echo [信息] 依赖已存在，跳过安装
    echo.
)

echo [信息] 启动 Web 管理后台...
echo.

node backend/app.js

pause
