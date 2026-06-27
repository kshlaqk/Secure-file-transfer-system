#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QAction>
#include <QMenu>
#include "FileTransferManager.h"
#include "PolicySyncService.h"
#include "SocketIOClient.h"
#include "DriverCommunicator.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 本地文件树
    void onLocalItemExpanded(QTreeWidgetItem* item);
    void onLocalItemDoubleClicked(QTreeWidgetItem* item, int column);
    
    // 文件操作
    void onBrowseLocal();
    void onRefresh();
    void onUpload();
    void onDownload();
    void onDelete();
    
    // 传输管理
    void onTransferAdded(const QString& fileName, const QString& targetId);
    void onTransferProgress(const QString& taskId, qint64 bytesSent, qint64 totalBytes);
    void onTransferCompleted(const QString& taskId, bool success);
    void onTransferError(const QString& taskId, const QString& error);
    void onTransferDeleted(const QString& taskId);
    
    // 传输控制
    void onCancelTransfer();
    void onResumeTransfer();
    
    // 待下载文件
    void onRefreshPendingFiles();  // 刷新待下载文件列表
    void onPendingFileDoubleClicked(QListWidgetItem* item);  // 双击待下载文件
    
    // 下载管理
    void onDownloadProgress(const QString& fileId, qint64 bytesReceived, qint64 totalBytes);
    void onDownloadCompleted(const QString& fileId, const QString& savePath, bool success);
    void onDownloadError(const QString& fileId, const QString& error);
    
    // 设置
    void onSettings();

protected:
    // 窗口关闭事件
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void createConnections();
    void updateLocalFileTree(const QString& path);
    void loadDirectoryChildren(QTreeWidgetItem* parentItem, const QString& dirPath);
    
    // === UI 组件 ===
    QSplitter* m_mainSplitter;
    
    // 左侧：本地文件区域
    QWidget* m_localPanel;
    QLabel* m_localPathLabel;
    QLineEdit* m_localPathEdit;
    QPushButton* m_localBrowseBtn;
    QTreeWidget* m_localFileTree;
    
    // 右侧：目标PC输入区域 + 待下载文件列表
    QWidget* m_remotePanel;
    QLabel* m_targetLabel;
    QLineEdit* m_targetEdit;
    QLabel* m_targetInfoLabel;
    QListWidget* m_pendingFilesList;  // 待下载文件列表（仅显示已完成传输的文件）
    QPushButton* m_refreshPendingBtn;  // 刷新待下载文件列表按钮
    
    // 中间：操作按钮
    QWidget* m_actionPanel;
    QPushButton* m_uploadBtn;
    QPushButton* m_downloadBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_refreshBtn;
    
    // 底部：传输任务列表
    QWidget* m_transferPanel;
    QLabel* m_transferLabel;
    QTableWidget* m_transferTable;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_clearCompletedBtn;
    
    // 菜单和工具栏
    QMenu* m_fileMenu;
    QMenu* m_transferMenu;
    QAction* m_exitAction;
    QAction* m_settingsAction;
    
    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_transferStatusLabel;
    
    // 业务逻辑
    FileTransferManager* m_transferManager;
    PolicySyncService* m_policySyncService;
    SocketIOClient* m_socketClient;
    DriverCommunicator* m_driverCommunicator;
    
    // 当前路径
    QString m_currentLocalPath;
    QString m_username;
    QString m_serverUrl;
};

#endif // MAINWINDOW_H




