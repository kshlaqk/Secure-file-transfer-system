@echo off
echo start server...
echo.

if not exist node_modules (
    echo installing...
    call npm install
    echo.
)

echo start server...
node src/app.js

pause
