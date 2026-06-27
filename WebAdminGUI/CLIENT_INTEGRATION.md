# 客户端集成说明

为了让客户端能够接收管理后台推送的策略和白名单更新，需要对客户端进行以下修改：

## 1. 修改策略同步 API 端点

修改 `PolicySyncThread.cpp` 中的 API 端点，从管理后台获取策略：

```cpp
// 在 PolicySyncThread.cpp 中
void PolicySyncThread::setApiEndpoint(const QString& endpoint)
{
    // 改为从管理后台获取策略
    // 例如: /api/admin/clients/{username}/policy
    QMutexLocker locker(&m_mutex);
    m_apiEndpoint = endpoint;
}
```

## 2. 添加 WebSocket 支持（推荐）

为了支持实时推送，建议在客户端添加 WebSocket 客户端来接收策略和白名单更新。

### 2.1 创建 WebSocket 客户端类

创建 `WebSocketClient.h` 和 `WebSocketClient.cpp`：

```cpp
// WebSocketClient.h
#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>

class WebSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    void connectToServer(const QString& url, const QString& username, const QString& machineId);
    void disconnectFromServer();
    
signals:
    void policyReceived(const QJsonObject& policyData);
    void whitelistReceived(const QJsonArray& whitelist);
    void logSyncRequested();
    void connected();
    void disconnected();
    void error(const QString& error);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onError(QAbstractSocket::SocketError error);
    
private:
    QWebSocket* m_webSocket;
    QString m_username;
    QString m_machineId;
};

#endif
```

### 2.2 在 MainWindow 中集成 WebSocket

在 `MainWindow.cpp` 的构造函数中：

```cpp
// 创建 WebSocket 客户端
m_webSocketClient = new WebSocketClient(this);
connect(m_webSocketClient, &WebSocketClient::policyReceived,
        m_policySyncService, &PolicySyncService::onPolicyReceived);
connect(m_webSocketClient, &WebSocketClient::whitelistReceived,
        this, &MainWindow::onWhitelistReceived);

// 连接 WebSocket
QSettings settings;
QString serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
QString username = settings.value("username", "").toString();
QString machineId = settings.value("machineId", "").toString();
m_webSocketClient->connectToServer(serverUrl, username, machineId);
```

## 3. 修改 PolicySyncThread 获取策略

修改 `PolicySyncThread::fetchPolicyFromWebService()` 方法，从管理后台获取策略：

```cpp
QJsonObject PolicySyncThread::fetchPolicyFromWebService()
{
    // 获取当前用户名
    QSettings settings;
    QString username = settings.value("username", "").toString();
    
    if (username.isEmpty()) {
        emit error("未登录");
        return QJsonObject();
    }
    
    // 构建 URL：从管理后台获取策略
    QString fullUrl = m_webServiceUrl + "/api/admin/clients/" + username + "/policy";
    
    // 使用 WebServiceClient 获取策略
    WebServiceClient client;
    QByteArray response;
    
    if (client.getRequest(fullUrl, response)) {
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                QJsonObject policy = result["policy"].toObject();
                // 转换为策略格式
                QJsonObject policyData;
                policyData["version"] = policy["policyVersion"];
                policyData["protectedExtensions"] = policy["protectedExtensions"];
                policyData["enableEncryption"] = policy["enableEncryption"];
                return policyData;
            }
        }
    }
    
    return QJsonObject();
}
```

## 4. 添加白名单管理功能

扩展 `DriverCommunicator` 类，添加白名单操作方法（参考之前的说明）。

## 5. 添加日志同步功能

在客户端添加日志同步功能：

```cpp
// 在 MainWindow 中
void MainWindow::onLogSyncRequested()
{
    // 从驱动获取日志
    QList<LogEntry> logs = m_driverCommunicator->getLogs();
    
    // 转换为 JSON 格式
    QJsonArray logArray;
    for (const LogEntry& log : logs) {
        QJsonObject logObj;
        logObj["timestamp"] = log.timestamp;
        logObj["processName"] = log.processName;
        logObj["filePath"] = log.filePath;
        logObj["action"] = log.action;
        logArray.append(logObj);
    }
    
    // 发送到服务器
    QSettings settings;
    QString username = settings.value("username", "").toString();
    QString serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
    
    QJsonObject request;
    request["username"] = username;
    request["logs"] = logArray;
    
    WebServiceClient client;
    QByteArray response;
    client.postRequest(serverUrl + "/api/admin/sync-logs", request, response);
}
```

## 注意事项

1. 客户端需要先登录才能获取策略
2. WebSocket 连接需要在登录成功后建立
3. 策略更新会立即推送给在线的客户端
4. 离线客户端会在上线后通过轮询获取最新策略
