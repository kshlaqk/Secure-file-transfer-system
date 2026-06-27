#!/bin/bash

echo "========================================"
echo "   Web 管理后台"
echo "========================================"
echo ""

if [ ! -d "node_modules" ]; then
    echo "[信息] 正在安装依赖..."
    echo ""
    npm install
    if [ $? -ne 0 ]; then
        echo ""
        echo "[错误] 依赖安装失败！"
        exit 1
    fi
    echo ""
    echo "[成功] 依赖安装完成！"
    echo ""
else
    echo "[信息] 依赖已存在，跳过安装"
    echo ""
fi

echo "[信息] 启动 Web 管理后台..."
echo ""

node backend/app.js
