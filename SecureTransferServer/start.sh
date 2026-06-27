#!/bin/bash

echo "启动安全文件传输服务器..."
echo ""

if [ ! -d "node_modules" ]; then
    echo "正在安装依赖..."
    npm install
    echo ""
fi

echo "启动服务器..."
node src/app.js
