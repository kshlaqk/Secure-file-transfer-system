#include "MainWindow.h"
#include "WebServiceClient.h"
#include "LoginDialog.h"
#include "SocketIOClient.h"
#include "DriverCommunicator.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFileIconProvider>
#include <QSettings>
#include <QInputDialog>
#include <QDebug>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_transferManager(new FileTransferManager(this))
    , m_policySyncService(new PolicySyncService(this))
    , m_socketClient(new SocketIOClient(this))
    , m_driverCommunicator(new DriverCommunicator(this))
{
    // 设置窗口属性，防止在创建时显示
    setAttribute(Qt::WA_DontShowOnScreen, true);
    
    // 先隐藏窗口，登录成功后再显示
    hide();
    
    // 显示登录对话框
    LoginDialog loginDialog(this);
    if (loginDialog.exec() != QDialog::Accepted) {
        // 用户取消登录，直接退出程序
        QApplication::exit(0);
        return;
    }
    
    // 获取登录信息
    m_username = loginDialog.getUsername();
    m_serverUrl = loginDialog.getServerUrl();
    QString machineId = loginDialog.getMachineId();
    
    // 设置窗口标题
    setWindowTitle(QString("安全文件传输客户端 - %1").arg(m_username));
    setMinimumSize(1000, 600);
    resize(1400, 800);
    
    setupUI();
    createConnections();
    
    // 初始化本地文件树
    m_currentLocalPath = QDir::homePath();
    m_localPathEdit->setText(m_currentLocalPath);
    updateLocalFileTree(m_currentLocalPath);
    
    // 连接策略同步服务信号
    connect(m_policySyncService, &PolicySyncService::policyUpdated,
            this, [](const QString& version) {
                qInfo() << "策略已更新，版本:" << version;
            });
    connect(m_policySyncService, &PolicySyncService::error,
            this, [](const QString& error) {
                qWarning() << "策略同步错误:" << error;
            });
    
    // 连接驱动通信器
    if (!m_driverCommunicator->connectToDriver()) {
        qWarning() << "Failed to connect to driver";
    }
    
    // 连接 Socket.io 客户端
    // 注意：不再在connected信号中调用registerClient，因为SocketIOClient会自动处理
    connect(m_socketClient, &SocketIOClient::connected, this, []() {
        qInfo() << "Socket.io连接已建立";
    });
    
    connect(m_socketClient, &SocketIOClient::disconnected, this, []() {
        qWarning() << "Socket.io disconnected";
    });
    
    connect(m_socketClient, &SocketIOClient::error, this, [](const QString& error) {
        qWarning() << "Socket.io error:" << error;
    });
    
    // 监听 Socket.io 事件
    connect(m_socketClient, &SocketIOClient::eventReceived, this, [this](const QString& event, const QJsonObject& data) {
        if (event == "requestLogs") {
            qInfo() << "Received requestLogs event, syncing logs";
            
            // 获取驱动日志
            QList<LogEntry> logs = m_driverCommunicator->getLogs();
            
            // 转换为 JSON 格式
            QJsonArray logsArray;
            for (const LogEntry& entry : logs) {
                QJsonObject logObj;
                logObj["timestamp"] = static_cast<qint64>(entry.Timestamp);
                logObj["processName"] = QString::fromWCharArray(entry.ProcessName);
                logObj["filePath"] = QString::fromWCharArray(entry.FilePath);
                logObj["action"] = static_cast<int>(entry.Action);
                logsArray.append(logObj);
            }
            
            // 发送 syncLogs 事件
            QJsonObject syncData;
            syncData["username"] = m_username;
            syncData["logs"] = logsArray;
            m_socketClient->emitEvent("syncLogs", syncData);
            
            qInfo() << "Synced" << logs.size() << "log entries to server";
        } else if (event == "policyUpdate") {
            qInfo() << "Received policyUpdate event";
            
            // 解析策略数据
            QString protectedExtensions = data.value("protectedExtensions").toString();
            bool enableEncryption = data.value("enableEncryption").toBool(false);
            QString version = data.value("version").toString();
            
            // 更新驱动策略
            if (m_driverCommunicator->updatePolicy(protectedExtensions, enableEncryption)) {
                qInfo() << "Policy updated successfully, version:" << version;
                
                // 发送确认消息
                QJsonObject confirmData;
                confirmData["username"] = m_username;
                confirmData["version"] = version;
                m_socketClient->emitEvent("policyUpdated", confirmData);
            } else {
                qWarning() << "Failed to update policy";
            }
        } else if (event == "whitelistUpdate") {
            qInfo() << "Received whitelistUpdate event";
            
            // 解析服务器推送的白名单数据
            QJsonArray whitelistArray = data.value("whitelist").toArray();
            QStringList newWhitelist;
            for (const QJsonValue& value : whitelistArray) {
                QString processPath = value.toString();
                if (!processPath.isEmpty()) {
                    newWhitelist.append(processPath);
                }
            }
            
            qInfo() << "Server whitelist contains" << newWhitelist.size() << "items";
            
            // 获取驱动当前的白名单
            QStringList currentWhitelist = m_driverCommunicator->getWhitelist();
            qInfo() << "Driver whitelist contains" << currentWhitelist.size() << "items";
            
            // 系统默认白名单（不应被删除）
            QStringList systemWhitelist = {
                "Windows\\System32\\notepad.exe",
                "whitelist\\client\\SecureTransferClient.exe",
                "Windows\\explorer.exe",
                "Windows\\System32\\svchost.exe",
                "Windows\\System32\\SearchProtocolHost.exe"
            };
            
            // 找出需要删除的项（在当前白名单中但不在新白名单中，且不是系统白名单）
            QStringList toRemove;
            for (const QString& currentPath : currentWhitelist) {
                // 检查是否是系统白名单（使用路径后缀匹配）
                bool isSystemPath = false;
                for (const QString& sysPath : systemWhitelist) {
                    // 使用路径后缀匹配，因为驱动中存储的可能是完整路径
                    QString normalizedCurrent = QString(currentPath).replace('\\', '/').toLower();
                    QString normalizedSys = QString(sysPath).replace('\\', '/').toLower();
                    
                    if (normalizedCurrent.endsWith(normalizedSys) || 
                        normalizedCurrent.contains(normalizedSys)) {
                        isSystemPath = true;
                        break;
                    }
                }
                
                // 如果不在新白名单中且不是系统路径，则需要删除
                bool inNewList = false;
                for (const QString& newPath : newWhitelist) {
                    // 使用不区分大小写的比较
                    QString normalizedCurrent = QString(currentPath).replace('\\', '/').toLower();
                    QString normalizedNew = QString(newPath).replace('\\', '/').toLower();
                    
                    if (normalizedCurrent == normalizedNew || 
                        normalizedCurrent.endsWith(normalizedNew) ||
                        normalizedNew.endsWith(normalizedCurrent)) {
                        inNewList = true;
                        break;
                    }
                }
                
                if (!isSystemPath && !inNewList) {
                    toRemove.append(currentPath);
                }
            }
            
            // 找出需要添加的项（在新白名单中但不在当前白名单中）
            QStringList toAdd;
            for (const QString& newPath : newWhitelist) {
                bool found = false;
                for (const QString& currentPath : currentWhitelist) {
                    // 使用不区分大小写的路径比较
                    QString normalizedCurrent = QString(currentPath).replace('\\', '/').toLower();
                    QString normalizedNew = QString(newPath).replace('\\', '/').toLower();
                    
                    if (normalizedCurrent == normalizedNew ||
                        normalizedCurrent.endsWith(normalizedNew) ||
                        normalizedNew.endsWith(normalizedCurrent)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    toAdd.append(newPath);
                }
            }
            
            qInfo() << "Whitelist diff: remove" << toRemove.size() << "items, add" << toAdd.size() << "items";
            
            // 执行删除操作
            bool removeSuccess = true;
            for (const QString& path : toRemove) {
                qInfo() << "Removing whitelist item:" << path;
                if (!m_driverCommunicator->removeWhitelistItem(path)) {
                    qWarning() << "Failed to remove whitelist item:" << path;
                    removeSuccess = false;
                }
            }
            
            // 执行添加操作
            bool addSuccess = true;
            for (const QString& path : toAdd) {
                qInfo() << "Adding whitelist item:" << path;
                if (!m_driverCommunicator->addWhitelistItem(path)) {
                    qWarning() << "Failed to add whitelist item:" << path;
                    addSuccess = false;
                }
            }
            
            if (removeSuccess && addSuccess) {
                qInfo() << "Whitelist updated successfully";
                
                // 发送确认消息
                QJsonObject confirmData;
                confirmData["username"] = m_username;
                m_socketClient->emitEvent("whitelistUpdated", confirmData);
            } else {
                qWarning() << "Failed to update whitelist completely";
            }
        }
    });
    
    // 在连接前先设置用户名和机器码，确保连接成功后能自动发送register事件
    qInfo() << "[客户端] 准备连接WebSocket，设置用户信息...";
    qInfo() << "[客户端] 用户名:" << m_username << "机器码:" << machineId;
    m_socketClient->registerClient(m_username, machineId);
    
    // 连接到服务器
    qInfo() << "[客户端] 开始连接WebSocket服务器:" << m_serverUrl;
    m_socketClient->connectToServer(m_serverUrl);
    
    // 登录成功，移除属性并显示窗口
    setAttribute(Qt::WA_DontShowOnScreen, false);
    show();
}

MainWindow::~MainWindow()
{
    // 停止策略同步服务
    if (m_policySyncService->isRunning()) {
        m_policySyncService->stopService();
    }
    
    // 断开 Socket.io 连接
    if (m_socketClient) {
        m_socketClient->disconnectFromServer();
    }
    
    // 断开驱动连接
    if (m_driverCommunicator) {
        m_driverCommunicator->disconnectFromDriver();
    }
}

void MainWindow::setupUI()
{
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
}

void MainWindow::setupCentralWidget()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // === 主分割器：上下布局 ===
    QSplitter* verticalSplitter = new QSplitter(Qt::Vertical);
    
    // === 上半部分：左右分栏（本地和远程文件） ===
    QSplitter* horizontalSplitter = new QSplitter(Qt::Horizontal);
    
    // --- 左侧：本地文件面板 ---
    m_localPanel = new QWidget();
    QVBoxLayout* localLayout = new QVBoxLayout(m_localPanel);
    localLayout->setContentsMargins(5, 5, 5, 5);
    
    QHBoxLayout* localPathLayout = new QHBoxLayout();
    m_localPathLabel = new QLabel("本地路径:");
    m_localPathEdit = new QLineEdit();
    m_localBrowseBtn = new QPushButton("浏览...");
    localPathLayout->addWidget(m_localPathLabel);
    localPathLayout->addWidget(m_localPathEdit);
    localPathLayout->addWidget(m_localBrowseBtn);
    
    m_localFileTree = new QTreeWidget();
    m_localFileTree->setHeaderLabel("文件名");
    m_localFileTree->setRootIsDecorated(true);
    m_localFileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    localLayout->addLayout(localPathLayout);
    localLayout->addWidget(m_localFileTree);
    
    // --- 右侧：目标PC输入面板 + 待下载文件列表 ---
    m_remotePanel = new QWidget();
    QVBoxLayout* remoteLayout = new QVBoxLayout(m_remotePanel);
    remoteLayout->setContentsMargins(5, 5, 5, 5);
    
    // 上半部分：目标账号输入
    QHBoxLayout* targetLayout = new QHBoxLayout();
    m_targetLabel = new QLabel("目标账号（名字+工号）:");
    m_targetEdit = new QLineEdit();
    m_targetEdit->setPlaceholderText("例如: 李明028");
    targetLayout->addWidget(m_targetLabel);
    targetLayout->addWidget(m_targetEdit);
    
    m_targetInfoLabel = new QLabel("输入目标账号后，选择本地文件进行发送");
    m_targetInfoLabel->setWordWrap(true);
    m_targetInfoLabel->setStyleSheet("color: gray;");
    
    // 下半部分：待下载文件列表（仅显示已完成传输的文件）
    QLabel* pendingFilesLabel = new QLabel("待下载文件（已完成传输）:");
    m_pendingFilesList = new QListWidget();
    m_pendingFilesList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pendingFilesList->setToolTip("显示已完整传输到服务器的文件，双击可下载");
    m_refreshPendingBtn = new QPushButton("刷新列表");
    
    QHBoxLayout* pendingHeaderLayout = new QHBoxLayout();
    pendingHeaderLayout->addWidget(pendingFilesLabel);
    pendingHeaderLayout->addStretch();
    pendingHeaderLayout->addWidget(m_refreshPendingBtn);
    
    remoteLayout->addLayout(targetLayout);
    remoteLayout->addWidget(m_targetInfoLabel);
    remoteLayout->addSpacing(10);
    remoteLayout->addLayout(pendingHeaderLayout);
    remoteLayout->addWidget(m_pendingFilesList);
    
    // --- 中间：操作按钮栏 ---
    m_actionPanel = new QWidget();
    QVBoxLayout* actionLayout = new QVBoxLayout(m_actionPanel);
    actionLayout->setContentsMargins(10, 50, 10, 50);
    
    m_uploadBtn = new QPushButton("发送 →");
    m_uploadBtn->setMinimumHeight(40);
    m_downloadBtn = new QPushButton("← 下载");
    m_downloadBtn->setMinimumHeight(40);
    m_downloadBtn->setEnabled(false);  // 禁用下载按钮
    m_downloadBtn->setToolTip("暂不支持下载功能");
    m_refreshBtn = new QPushButton("刷新");
    m_refreshBtn->setMinimumHeight(30);
    
    actionLayout->addStretch();
    actionLayout->addWidget(m_uploadBtn);
    actionLayout->addWidget(m_downloadBtn);
    actionLayout->addSpacing(20);
    actionLayout->addWidget(m_refreshBtn);
    actionLayout->addStretch();
    
    horizontalSplitter->addWidget(m_localPanel);
    horizontalSplitter->insertWidget(1, m_actionPanel);
    horizontalSplitter->addWidget(m_remotePanel);
    horizontalSplitter->setStretchFactor(0, 1);
    horizontalSplitter->setStretchFactor(1, 0);
    horizontalSplitter->setStretchFactor(2, 1);
    horizontalSplitter->setCollapsible(1, false);
    
    // === 下半部分：传输任务列表 ===
    m_transferPanel = new QWidget();
    QVBoxLayout* transferLayout = new QVBoxLayout(m_transferPanel);
    transferLayout->setContentsMargins(5, 5, 5, 5);
    
    QHBoxLayout* transferHeaderLayout = new QHBoxLayout();
    m_transferLabel = new QLabel("传输任务:");
    transferHeaderLayout->addWidget(m_transferLabel);
    transferHeaderLayout->addStretch();
    
    m_pauseBtn = new QPushButton("暂停");
    m_resumeBtn = new QPushButton("继续");
    m_cancelBtn = new QPushButton("取消");
    m_clearCompletedBtn = new QPushButton("清除已完成");
    
    transferHeaderLayout->addWidget(m_pauseBtn);
    transferHeaderLayout->addWidget(m_resumeBtn);
    transferHeaderLayout->addWidget(m_cancelBtn);
    transferHeaderLayout->addWidget(m_clearCompletedBtn);
    
    m_transferTable = new QTableWidget(0, 5);
    m_transferTable->setHorizontalHeaderLabels(
        QStringList() << "文件名" << "目标PC" << "进度" << "速度" << "状态");
    m_transferTable->horizontalHeader()->setStretchLastSection(true);
    m_transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transferTable->setColumnWidth(0, 300);
    m_transferTable->setColumnWidth(1, 80);
    m_transferTable->setColumnWidth(2, 200);
    m_transferTable->setColumnWidth(3, 100);
    m_transferTable->setColumnWidth(4, 100);
    
    transferLayout->addLayout(transferHeaderLayout);
    transferLayout->addWidget(m_transferTable);
    
    verticalSplitter->addWidget(horizontalSplitter);
    verticalSplitter->addWidget(m_transferPanel);
    verticalSplitter->setStretchFactor(0, 3);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setSizes(QList<int>() << 600 << 200);
    
    mainLayout->addWidget(verticalSplitter);
    setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar()
{
    m_fileMenu = menuBar()->addMenu("文件(&F)");
    m_exitAction = new QAction("退出", this);
    m_fileMenu->addAction(m_exitAction);
    
    QMenu* transferMenu = menuBar()->addMenu("传输(&T)");
    QAction* pauseAllAction = new QAction("暂停全部", this);
    QAction* resumeAllAction = new QAction("继续全部", this);
    QAction* clearAllAction = new QAction("清除全部", this);
    transferMenu->addAction(pauseAllAction);
    transferMenu->addAction(resumeAllAction);
    transferMenu->addSeparator();
    transferMenu->addAction(clearAllAction);
    
    QMenu* settingsMenu = menuBar()->addMenu("设置(&S)");
    m_settingsAction = new QAction("设置...", this);
    settingsMenu->addAction(m_settingsAction);
}

void MainWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar("主工具栏");
    toolBar->addAction(m_settingsAction);
    toolBar->addSeparator();
    toolBar->addAction(new QAction("上传", this));
    toolBar->addAction(new QAction("下载", this));
    toolBar->addSeparator();
    toolBar->addAction(new QAction("刷新", this));
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("就绪");
    m_transferStatusLabel = new QLabel("传输中: 0");
    
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_transferStatusLabel);
}

void MainWindow::createConnections()
{
    connect(m_localBrowseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseLocal);
    connect(m_localPathEdit, &QLineEdit::returnPressed, [this]() {
        updateLocalFileTree(m_localPathEdit->text());
    });
    connect(m_localFileTree, &QTreeWidget::itemExpanded,
            this, &MainWindow::onLocalItemExpanded);
    connect(m_localFileTree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onLocalItemDoubleClicked);
    
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUpload);
    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownload);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefresh);
    
    // 待下载文件列表按钮
    connect(m_refreshPendingBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPendingFiles);
    connect(m_pendingFilesList, &QListWidget::itemDoubleClicked, this, &MainWindow::onPendingFileDoubleClicked);
    
    // 传输控制按钮
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelTransfer);
    connect(m_resumeBtn, &QPushButton::clicked, this, &MainWindow::onResumeTransfer);
    
    // 传输管理器信号
    connect(m_transferManager, &FileTransferManager::transferAdded,
            this, &MainWindow::onTransferAdded);
    connect(m_transferManager, &FileTransferManager::transferProgress,
            this, &MainWindow::onTransferProgress);
    connect(m_transferManager, &FileTransferManager::transferCompleted,
            this, &MainWindow::onTransferCompleted);
    connect(m_transferManager, &FileTransferManager::transferError,
            this, &MainWindow::onTransferError);
    connect(m_transferManager, &FileTransferManager::transferDeleted,
            this, &MainWindow::onTransferDeleted);
    
    // 下载相关信号
    connect(m_transferManager, &FileTransferManager::downloadProgress,
            this, &MainWindow::onDownloadProgress);
    connect(m_transferManager, &FileTransferManager::downloadCompleted,
            this, &MainWindow::onDownloadCompleted);
    connect(m_transferManager, &FileTransferManager::downloadError,
            this, &MainWindow::onDownloadError);
    
    connect(m_clearCompletedBtn, &QPushButton::clicked, [this]() {
        for (int i = m_transferTable->rowCount() - 1; i >= 0; --i) {
            QTableWidgetItem* statusItem = m_transferTable->item(i, 4);
            if (statusItem && (statusItem->text() == "完成" || statusItem->text() == "失败")) {
                m_transferTable->removeRow(i);
            }
        }
    });
    
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);
}

void MainWindow::onBrowseLocal()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "选择本地目录", m_localPathEdit->text());
    if (!dir.isEmpty()) {
        m_localPathEdit->setText(dir);
        updateLocalFileTree(dir);
    }
}

void MainWindow::updateLocalFileTree(const QString& path)
{
    m_localFileTree->clear();
    
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }
    
    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot, 
        QDir::Name | QDir::DirsFirst);
    
    QFileIconProvider iconProvider;
    
    for (const QFileInfo& info : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_localFileTree);
        item->setText(0, info.fileName());
        item->setIcon(0, iconProvider.icon(info));
        
        if (info.isDir()) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            item->setData(0, Qt::UserRole, info.absoluteFilePath());
            // 添加一个占位子项，用于显示展开指示器（懒加载时替换）
            new QTreeWidgetItem(item);
        } else {
            item->setData(0, Qt::UserRole, info.absoluteFilePath());
            item->setToolTip(0, QString("大小: %1 字节").arg(info.size()));
        }
    }
    
    m_currentLocalPath = path;
}

void MainWindow::loadDirectoryChildren(QTreeWidgetItem* parentItem, const QString& dirPath)
{
    parentItem->takeChildren();
    
    QDir dir(dirPath);
    if (!dir.exists()) {
        return;
    }
    
    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::Name | QDir::DirsFirst);
    
    QFileIconProvider iconProvider;
    
    for (const QFileInfo& info : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);
        item->setText(0, info.fileName());
        item->setIcon(0, iconProvider.icon(info));
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        
        if (info.isDir()) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            // 懒加载占位子项
            new QTreeWidgetItem(item);
        } else {
            item->setToolTip(0, QString("大小: %1 字节").arg(info.size()));
        }
    }
}

void MainWindow::onLocalItemExpanded(QTreeWidgetItem* item)
{
    const QString dirPath = item->data(0, Qt::UserRole).toString();
    if (dirPath.isEmpty()) {
        return;
    }
    
    QFileInfo info(dirPath);
    if (!info.isDir()) {
        return;
    }
    
    loadDirectoryChildren(item, dirPath);
}

void MainWindow::onLocalItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    
    QFileInfo info(path);
    if (info.isDir()) {
        m_localPathEdit->setText(path);
        updateLocalFileTree(path);
    }
}

void MainWindow::onUpload()
{
    // 获取目标账号（名字+工号）
    QString targetId = m_targetEdit->text().trimmed();
    if (targetId.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入目标账号（名字+工号，例如: 李明028）");
        return;
    }
    
    // 获取选中的本地文件
    QList<QTreeWidgetItem*> selected = m_localFileTree->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请选择要发送的文件");
        return;
    }
    
    // 发送文件
    for (QTreeWidgetItem* item : selected) {
        QString fileName = item->text(0);
        QString localFilePath = m_currentLocalPath + "/" + fileName;
        
        // 检查是否为文件（而非目录）
        QFileInfo fileInfo(localFilePath);
        if (fileInfo.isDir()) {
            QMessageBox::information(this, "提示", 
                QString("暂不支持发送目录: %1").arg(fileName));
            continue;
        }
        
        m_transferManager->sendFile(localFilePath, targetId);
    }
}

void MainWindow::onDownload()
{
    // 暂不支持下载功能
    QMessageBox::information(this, "提示", "暂不支持下载功能");
}

void MainWindow::onDelete()
{
    // 删除传输任务
    QList<QTableWidgetItem*> selected = m_transferTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请选择要删除的传输任务");
        return;
    }
    
    // 获取任务ID（从选中行的第一列数据中获取）
    QTableWidgetItem* firstItem = selected.first();
    int row = firstItem->row();
    QTableWidgetItem* item = m_transferTable->item(row, 0);
    
    if (item) {
        QString taskId = item->data(Qt::UserRole).toString();
        if (!taskId.isEmpty()) {
            int ret = QMessageBox::question(this, "确认删除", 
                "确定要删除这个传输任务吗？\n删除后将从任务列表中移除，并向服务器发送删除请求。",
                QMessageBox::Yes | QMessageBox::No);
            
            if (ret == QMessageBox::Yes) {
                m_transferManager->deleteTask(taskId);
            }
        }
    }
}

void MainWindow::onRefresh()
{
    updateLocalFileTree(m_currentLocalPath);
    // TODO: 刷新远程文件树
}

void MainWindow::onTransferAdded(const QString& fileName, const QString& targetId)
{
    int row = m_transferTable->rowCount();
    m_transferTable->insertRow(row);
    
    m_transferTable->setItem(row, 0, new QTableWidgetItem(fileName));
    m_transferTable->setItem(row, 1, new QTableWidgetItem(targetId));
    
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    m_transferTable->setCellWidget(row, 2, progressBar);
    
    m_transferTable->setItem(row, 3, new QTableWidgetItem("0 KB/s"));
    m_transferTable->setItem(row, 4, new QTableWidgetItem("等待中"));
    
    m_transferTable->item(row, 0)->setData(Qt::UserRole, 
        m_transferManager->getLastTaskId());
}

void MainWindow::onTransferProgress(const QString& taskId, 
                                    qint64 bytesSent, 
                                    qint64 totalBytes)
{
    for (int i = 0; i < m_transferTable->rowCount(); ++i) {
        QTableWidgetItem* item = m_transferTable->item(i, 0);
        if (item && item->data(Qt::UserRole).toString() == taskId) {
            QProgressBar* progressBar = 
                qobject_cast<QProgressBar*>(m_transferTable->cellWidget(i, 2));
            if (progressBar) {
                int percent = totalBytes > 0 ? 
                    (bytesSent * 100 / totalBytes) : 0;
                progressBar->setValue(percent);
            }
            
            QTableWidgetItem* statusItem = m_transferTable->item(i, 4);
            if (statusItem) {
                statusItem->setText("传输中");
            }
            break;
        }
    }
}

void MainWindow::onTransferCompleted(const QString& taskId, bool success)
{
    for (int i = 0; i < m_transferTable->rowCount(); ++i) {
        QTableWidgetItem* item = m_transferTable->item(i, 0);
        if (item && item->data(Qt::UserRole).toString() == taskId) {
            QTableWidgetItem* statusItem = m_transferTable->item(i, 4);
            if (statusItem) {
                statusItem->setText(success ? "完成" : "失败");
                statusItem->setForeground(success ? 
                    QBrush(Qt::darkGreen) : QBrush(Qt::red));
            }
            
            QProgressBar* progressBar = 
                qobject_cast<QProgressBar*>(m_transferTable->cellWidget(i, 2));
            if (progressBar) {
                progressBar->setValue(100);
            }
            break;
        }
    }
}

void MainWindow::onTransferError(const QString& taskId, const QString& error)
{
    for (int i = 0; i < m_transferTable->rowCount(); ++i) {
        QTableWidgetItem* item = m_transferTable->item(i, 0);
        if (item && item->data(Qt::UserRole).toString() == taskId) {
            QTableWidgetItem* statusItem = m_transferTable->item(i, 4);
            if (statusItem) {
                statusItem->setText("失败");
                statusItem->setForeground(QBrush(Qt::red));
                statusItem->setToolTip(error);
            }
            break;
        }
    }
}

void MainWindow::onTransferDeleted(const QString& taskId)
{
    // 从传输任务列表中移除对应的行
    for (int i = 0; i < m_transferTable->rowCount(); ++i) {
        QTableWidgetItem* item = m_transferTable->item(i, 0);
        if (item && item->data(Qt::UserRole).toString() == taskId) {
            m_transferTable->removeRow(i);
            qInfo() << "Transfer task removed from UI:" << taskId;
            break;
        }
    }
}

void MainWindow::onCancelTransfer()
{
    // 获取当前选中的传输任务
    QList<QTableWidgetItem*> selected = m_transferTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请选择要取消的传输任务");
        return;
    }
    
    // 获取任务ID（从选中行的第一列数据中获取）
    QTableWidgetItem* firstItem = selected.first();
    int row = firstItem->row();
    QTableWidgetItem* item = m_transferTable->item(row, 0);
    
    if (item) {
        QString taskId = item->data(Qt::UserRole).toString();
        if (!taskId.isEmpty()) {
            m_transferManager->cancelTask(taskId);
        }
    }
}

void MainWindow::onResumeTransfer()
{
    // 获取当前选中的传输任务
    QList<QTableWidgetItem*> selected = m_transferTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请选择要继续的传输任务");
        return;
    }
    
    // 获取任务ID
    QTableWidgetItem* firstItem = selected.first();
    int row = firstItem->row();
    QTableWidgetItem* item = m_transferTable->item(row, 0);
    
    if (item) {
        QString taskId = item->data(Qt::UserRole).toString();
        if (!taskId.isEmpty()) {
            QString newTaskId = m_transferManager->resumeTask(taskId);
            if (!newTaskId.isEmpty()) {
                // 更新任务ID（如果继续传输返回了新的任务ID）
                // 这里暂时使用原任务ID
            }
        }
    }
}

void MainWindow::onRefreshPendingFiles()
{
    // 从服务器获取已完成传输的文件列表
    QSettings settings;
    QString serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
    QString username = settings.value("username", "").toString();  // 获取当前登录的用户名
    
    // 添加 targetId 参数，只获取发送给当前用户的文件
    QString apiUrl = serverUrl + "/api/files/completed?targetId=" + QUrl::toPercentEncoding(username);
    
    WebServiceClient client;
    QByteArray response;
    
    if (client.getRequest(apiUrl, response)) {
        // 解析响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                // 清空列表
                m_pendingFilesList->clear();
                
                // 解析文件列表
                QJsonArray files = result["files"].toArray();
                for (const QJsonValue& value : files) {
                    QJsonObject fileObj = value.toObject();
                    
                    // 只显示状态为"已完成"的文件
                    QString status = fileObj["status"].toString();
                    if (status == "completed" || status == "已完成") {
                        QString fileName = fileObj["fileName"].toString();
                        QString senderId = fileObj["senderId"].toString();  // 发送者ID
                        qint64 fileSize = fileObj["fileSize"].toVariant().toLongLong();  // 文件大小
                        
                        // 格式化文件大小
                        QString sizeStr;
                        if (fileSize < 1024) {
                            sizeStr = QString::number(fileSize) + " B";
                        } else if (fileSize < 1024 * 1024) {
                            sizeStr = QString::number(fileSize / 1024.0, 'f', 2) + " KB";
                        } else {
                            sizeStr = QString::number(fileSize / (1024.0 * 1024.0), 'f', 2) + " MB";
                        }
                        
                        // 创建列表项，显示文件名和发送者信息
                        QString displayText = QString("%1 (来自: %2)").arg(fileName).arg(senderId);
                        QListWidgetItem* item = new QListWidgetItem(displayText, m_pendingFilesList);
                        
                        // 存储文件信息到 item 的 data 中
                        QJsonObject fileData;
                        fileData["fileName"] = fileName;
                        fileData["senderId"] = senderId;
                        fileData["fileSize"] = fileSize;
                        fileData["fileId"] = fileObj["fileId"].toString();  // 服务器端的文件ID
                        item->setData(Qt::UserRole, QVariant::fromValue(fileData));
                        
                        // 设置工具提示
                        item->setToolTip(QString("文件名: %1\n发送者: %2\n大小: %3")
                                        .arg(fileName).arg(senderId).arg(sizeStr));
                    }
                }
                
                m_statusLabel->setText(QString("已加载 %1 个待下载文件").arg(m_pendingFilesList->count()));
            } else {
                QMessageBox::warning(this, "错误", 
                    "获取文件列表失败: " + result["error"].toString());
            }
        } else {
            QMessageBox::warning(this, "错误", "服务器响应格式错误");
        }
    } else {
        QMessageBox::warning(this, "错误", 
            "无法连接到服务器: " + client.getLastError());
    }
}

void MainWindow::onPendingFileDoubleClicked(QListWidgetItem* item)
{
    // 获取文件信息
    QJsonObject fileData = item->data(Qt::UserRole).toJsonObject();
    QString fileName = fileData["fileName"].toString();
    QString fileId = fileData["fileId"].toString();
    qint64 fileSize = fileData["fileSize"].toVariant().toLongLong();
    
    // 固定保存路径：C:\whitelist\protect
    QString protectDir = "C:\\whitelist\\protect";
    QDir dir;
    
    // 确保目录存在
    if (!dir.exists(protectDir)) {
        if (!dir.mkpath(protectDir)) {
            QMessageBox::warning(this, "错误", 
                QString("无法创建目录: %1\n请检查权限或手动创建该目录").arg(protectDir));
            return;
        }
    }
    
    // 构建完整文件路径
    QString savePath = protectDir + "\\" + fileName;
    
    // 检查文件是否已存在
    QFileInfo fileInfo(savePath);
    if (fileInfo.exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "文件已存在", 
            QString("文件 %1 已存在，是否覆盖？").arg(fileName),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
    }
    
    // 调用下载功能
    m_transferManager->downloadFile(fileId, savePath);
    
    // 显示下载提示
    m_statusLabel->setText(QString("正在下载: %1 到 %2").arg(fileName).arg(protectDir));
}

void MainWindow::onSettings()
{
    // 显示设置对话框，配置Web服务URL和策略同步
    QSettings settings;
    QString currentUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
    
    bool ok;
    QString url = QInputDialog::getText(this, "设置", 
        "Web服务URL:", QLineEdit::Normal, currentUrl, &ok);
    
    if (ok && !url.isEmpty()) {
        settings.setValue("webServiceUrl", url);
        
        // 启动策略同步服务
        if (!m_policySyncService->isRunning()) {
            m_policySyncService->startService(url, 5000);
            QMessageBox::information(this, "成功", 
                QString("策略同步服务已启动\nURL: %1").arg(url));
        } else {
            m_policySyncService->stopService();
            m_policySyncService->startService(url, 5000);
            QMessageBox::information(this, "成功", 
                QString("策略同步服务已重新启动\nURL: %1").arg(url));
        }
    }
}

void MainWindow::onDownloadProgress(const QString& fileId, qint64 bytesReceived, qint64 totalBytes)
{
    // 更新状态栏显示下载进度
    if (totalBytes > 0) {
        double percent = (bytesReceived * 100.0) / totalBytes;
        QString sizeStr;
        if (totalBytes < 1024) {
            sizeStr = QString("%1 B").arg(totalBytes);
        } else if (totalBytes < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(totalBytes / 1024.0, 0, 'f', 2);
        } else {
            sizeStr = QString("%1 MB").arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2);
        }
        m_statusLabel->setText(QString("下载中: %1% (%2 / %3)")
                              .arg(percent, 0, 'f', 1)
                              .arg(bytesReceived)
                              .arg(sizeStr));
    } else {
        m_statusLabel->setText(QString("下载中: %1 字节").arg(bytesReceived));
    }
}

void MainWindow::onDownloadCompleted(const QString& fileId, const QString& savePath, bool success)
{
    
    if (success) {
        QFileInfo fileInfo(savePath);
        QString fileName = fileInfo.fileName();
        m_statusLabel->setText(QString("下载完成: %1").arg(fileName));
        
        // 通知服务器删除文件
        qDebug() << "下载完成，通知服务器删除文件，fileId:" << fileId;
        m_transferManager->confirmDownload(fileId);
        
        QMessageBox::information(this, "下载完成", 
            QString("文件已成功下载到:\n%1").arg(savePath));
    } else {
        m_statusLabel->setText("下载失败");
        QMessageBox::warning(this, "下载失败", "文件下载失败");
    }
}

void MainWindow::onDownloadError(const QString& fileId, const QString& error)
{
    Q_UNUSED(fileId)
    m_statusLabel->setText(QString("下载错误: %1").arg(error));
    QMessageBox::warning(this, "下载错误", 
        QString("文件下载时发生错误:\n%1").arg(error));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 发送离线通知到服务器
    if (!m_username.isEmpty() && !m_serverUrl.isEmpty()) {
        QString apiUrl = m_serverUrl + "/api/pc/offline";
        QJsonObject request;
        request["username"] = m_username;
        
        WebServiceClient client;
        QByteArray response;
        client.setTimeout(2000);
        client.postRequest(apiUrl, request, response);
        
        qInfo() << "已发送离线通知到服务器";
    }
    
    // 断开 Socket.io 连接
    if (m_socketClient && m_socketClient->isConnected()) {
        m_socketClient->disconnectFromServer();
    }
    
    // 停止策略同步服务
    if (m_policySyncService->isRunning()) {
        m_policySyncService->stopService();
    }
    
    // 断开驱动连接
    if (m_driverCommunicator) {
        m_driverCommunicator->disconnectFromDriver();
    }
    
    event->accept();
}