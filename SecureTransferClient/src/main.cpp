#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QLoggingCategory>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QStandardPaths>

// 自定义消息处理器：输出到日志文件
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    QString logLevel;
    switch (type) {
    case QtDebugMsg:
        logLevel = "DEBUG";
        break;
    case QtInfoMsg:
        logLevel = "INFO";
        break;
    case QtWarningMsg:
        logLevel = "WARN";
        break;
    case QtCriticalMsg:
        logLevel = "ERROR";
        break;
    case QtFatalMsg:
        logLevel = "FATAL";
        break;
    }
    
    QString logMessage = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(logLevel)
        .arg(msg);
    
    // 输出到日志文件
    QString logDir = QDir::currentPath() + "/logs";
    QDir().mkpath(logDir);
    
    QString logFileName = QString("%1/client_%2.log")
        .arg(logDir)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));
    
    QFile logFile(logFileName);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream(&logFile) << logMessage << Qt::endl;
        logFile.close();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 安装自定义消息处理器
    qInstallMessageHandler(customMessageHandler);
    
    // 设置应用程序信息
    app.setApplicationName("SecureTransferClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SecureTransfer");
    
    qInfo() << "========================================";
    qInfo() << "SecureTransferClient 启动";
    qInfo() << "========================================";
    
    // 设置样式（可选）
    // app.setStyle(QStyleFactory::create("Fusion"));
    
    // 创建主窗口（构造函数中会显示登录对话框）
    // 如果登录失败，构造函数中会调用QApplication::quit()退出程序
    // 如果登录成功，构造函数中会显示窗口
    MainWindow window;
    
    return app.exec();
}




