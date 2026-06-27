#include "SocketIOClient.h"
#include <QUrl>
#include <QDebug>
#include <QJsonArray>
#include <QAbstractSocket>
#include <QTimer>
#include <QDateTime>

SocketIOClient::SocketIOClient(QObject *parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket("", QWebSocketProtocol::VersionLatest, this))
    , m_heartbeatTimer(new QTimer(this))
    , m_connected(false)
    , m_handshakeCompleted(false)  // 初始化握手标志
    , m_pingInterval(25000)  // 25秒心跳
{
    // 连接 WebSocket 信号
    connect(m_webSocket, &QWebSocket::connected, this, &SocketIOClient::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &SocketIOClient::onWebSocketDisconnected);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SocketIOClient::onWebSocketError);
    connect(m_webSocket, &QWebSocket::textMessageReceived,
            this, &SocketIOClient::onWebSocketTextMessageReceived);

    // 心跳定时器
    m_heartbeatTimer->setSingleShot(false);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &SocketIOClient::onHeartbeatTimeout);
}

SocketIOClient::~SocketIOClient()
{
    disconnectFromServer();
}

void SocketIOClient::connectToServer(const QString& serverUrl)
{
    if (m_connected) {
        qWarning() << "Already connected to server";
        return;
    }

    m_serverUrl = serverUrl;
    
    // 将 HTTP URL 转换为 WebSocket URL
    QString wsUrl = serverUrl;
    wsUrl.replace("http://", "ws://");
    wsUrl.replace("https://", "wss://");
    
    // Socket.io 连接路径（简化版本，直接连接 WebSocket）
    // 注意：实际 Socket.io 可能需要先进行 HTTP 握手，这里使用简化版本
    if (!wsUrl.endsWith("/")) {
        wsUrl += "/";
    }
    wsUrl += "socket.io/?EIO=4&transport=websocket";
    
    qInfo() << "Connecting to Socket.io server:" << wsUrl;
    m_webSocket->open(QUrl(wsUrl));
}

void SocketIOClient::disconnectFromServer()
{
    if (m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
    }
    
    if (m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
    }
    
    m_connected = false;
    m_handshakeCompleted = false;  // 重置握手标志
}

bool SocketIOClient::isConnected() const
{
    return m_connected && m_webSocket->state() == QAbstractSocket::ConnectedState;
}

void SocketIOClient::registerClient(const QString& username, const QString& machineId)
{
    qInfo() << "[SocketIOClient] registerClient 被调用";
    qInfo() << "[SocketIOClient] 用户名:" << username << "机器码:" << machineId;
    
    m_username = username;
    m_machineId = machineId;
    
    // 如果握手已完成，立即发送register事件
    if (m_handshakeCompleted && isConnected()) {
        qInfo() << "[SocketIOClient] 握手已完成，立即发送register事件";
        QJsonObject registerData;
        registerData["username"] = username;
        registerData["machineId"] = machineId;
        emitEvent("register", registerData);
    } else {
        qInfo() << "[SocketIOClient] 等待握手完成后自动注册";
    }
}

void SocketIOClient::emitEvent(const QString& event, const QJsonObject& data)
{
    if (!isConnected()) {
        qWarning() << "[SocketIOClient] ✗ 无法发送事件: 未连接";
        qWarning() << "[SocketIOClient] 事件名称:" << event;
        return;
    }
    
    // 检查握手是否完成
    if (!m_handshakeCompleted) {
        qWarning() << "[SocketIOClient] ✗ 无法发送事件: Socket.io握手未完成";
        qWarning() << "[SocketIOClient] 事件名称:" << event;
        qWarning() << "[SocketIOClient] 请等待握手完成后再发送事件";
        return;
    }
    
    QString message = encodeSocketIOMessage(event, data);
    qInfo() << "[SocketIOClient] ===== 发送事件 =====";
    qInfo() << "[SocketIOClient] 事件名称:" << event;
    qInfo() << "[SocketIOClient] 事件数据:" << QJsonDocument(data).toJson(QJsonDocument::Compact);
    qInfo() << "[SocketIOClient] 编码后的消息:" << message;
    
    m_webSocket->sendTextMessage(message);
    qInfo() << "[SocketIOClient] ✓ 事件已发送";
    qInfo() << "[SocketIOClient] ====================";
}

void SocketIOClient::onWebSocketConnected()
{
    qInfo() << "========================================";
    qInfo() << "[SocketIOClient] ===== WebSocket连接成功 =====";
    qInfo() << "[SocketIOClient] 连接时间:" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // 重置握手标志
    m_handshakeCompleted = false;
    
    // 注意：Socket.io协议中，客户端不应该主动发送ping
    // 只需要响应服务器发送的ping（"2"）消息，回复pong（"3"）
    // 因此不需要启动心跳定时器
    
    m_connected = true;
    // 注意：这里不立即发送connected信号，等待握手完成后再发送
    qInfo() << "[SocketIOClient] 等待服务器握手消息...";
    qInfo() << "========================================";
}

void SocketIOClient::onWebSocketDisconnected()
{
    qInfo() << "WebSocket disconnected";
    
    m_heartbeatTimer->stop();
    m_connected = false;
    m_handshakeCompleted = false;  // 重置握手标志
    emit disconnected();
}

void SocketIOClient::onWebSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = QString("WebSocket error: %1").arg(error);
    qWarning() << errorString;
    emit this->error(errorString);
}

void SocketIOClient::onWebSocketTextMessageReceived(const QString& message)
{
    qDebug() << "Received Socket.io message:" << message;
    
    // Socket.io 协议解析
    parseSocketIOMessage(message);
}

void SocketIOClient::onHeartbeatTimeout()
{
    // Socket.io协议中，客户端不应该主动发送ping
    // 只需要响应服务器发送的ping消息
    // 这个方法保留但不执行任何操作
}

void SocketIOClient::sendHeartbeat()
{
    // Socket.io协议中，客户端不应该主动发送ping
    // 只需要响应服务器发送的ping（"2"）消息，回复pong（"3"）
    // 这个方法保留但不执行任何操作
}

void SocketIOClient::parseSocketIOMessage(const QString& message)
{
    if (message.isEmpty()) {
        return;
    }
    
    // Socket.io 协议格式：
    // "0{...}" - 初始握手消息，包含session ID等信息
    // "40" 或 "40{...}" - 命名空间连接确认
    // "2" - ping (需要响应 pong "3")
    // "3" - pong (心跳响应)
    // "42["event",{data}]" - 事件消息
    
    if (message.startsWith("0")) {
        // 初始握手消息：0{"sid":"...","upgrades":[],"pingInterval":25000,"pingTimeout":20000,"maxPayload":1000000}
        qInfo() << "[SocketIOClient] 收到Socket.io握手消息";
        QString jsonStr = message.mid(1);  // 移除 "0" 前缀
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject handshake = doc.object();
            QString sid = handshake.value("sid").toString();
            int pingInterval = handshake.value("pingInterval").toInt(25000);
            int pingTimeout = handshake.value("pingTimeout").toInt(20000);
            
            qInfo() << "[SocketIOClient] 握手信息 - Session ID:" << sid;
            qInfo() << "[SocketIOClient] 握手信息 - Ping Interval:" << pingInterval << "ms";
            qInfo() << "[SocketIOClient] 握手信息 - Ping Timeout:" << pingTimeout << "ms";
            qInfo() << "[SocketIOClient] 注意：客户端只需响应服务器的ping，不需要主动发送ping";
            
            // 保存pingInterval和pingTimeout（虽然不使用，但保留用于日志）
            if (pingInterval > 0) {
                m_pingInterval = pingInterval;
            }
            
            // 发送 "40" 连接到默认命名空间
            qInfo() << "[SocketIOClient] 发送命名空间连接请求: 40";
            m_webSocket->sendTextMessage("40");
        } else {
            qWarning() << "[SocketIOClient] 无法解析握手消息:" << error.errorString();
        }
    } else if (message == "40" || message.startsWith("40")) {
        // 命名空间连接确认（可能是 "40" 或 "40{\"sid\":\"...\"}"）
        qInfo() << "[SocketIOClient] ✓ Socket.io命名空间连接确认";
        
        // 如果消息包含JSON数据，解析它
        if (message.length() > 2) {
            QString jsonStr = message.mid(2);  // 移除 "40" 前缀
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject data = doc.object();
                QString sid = data.value("sid").toString();
                if (!sid.isEmpty()) {
                    qInfo() << "[SocketIOClient] 命名空间 Session ID:" << sid;
                }
            }
        }
        
        m_handshakeCompleted = true;
        
        // 握手完成，发送connected信号
        emit connected();
        qInfo() << "[SocketIOClient] 已发送connected信号";
        
        // 尝试自动注册
        tryAutoRegister();
    } else if (message == "2") {
        // ping 消息，需要立即响应 pong
        qInfo() << "[SocketIOClient] 收到服务器 ping，立即响应 pong";
        m_webSocket->sendTextMessage("3");
        qInfo() << "[SocketIOClient] ✓ 已发送 pong 响应";
    } else if (message == "3") {
        // pong 响应（客户端不应该收到这个，因为客户端不发送ping）
        qDebug() << "Received pong (unexpected)";
    } else if (message.startsWith("42")) {
        // 事件消息：42["event",{data}]
        QString jsonStr = message.mid(2);  // 移除 "42" 前缀
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
        
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "Failed to parse Socket.io event message:" << error.errorString();
            return;
        }
        
        if (doc.isArray() && doc.array().size() >= 2) {
            QJsonArray arr = doc.array();
            QString event = arr[0].toString();
            QJsonObject data = arr[1].toObject();
            
            qInfo() << "Received Socket.io event:" << event;
            emit eventReceived(event, data);
        }
    } else {
        qWarning() << "[SocketIOClient] 未知的Socket.io消息格式:" << message;
    }
}

QString SocketIOClient::encodeSocketIOMessage(const QString& event, const QJsonObject& data)
{
    // Socket.io 协议格式：42["event",{data}]
    QJsonArray arr;
    arr.append(event);
    arr.append(data);
    
    QJsonDocument doc(arr);
    return "42" + doc.toJson(QJsonDocument::Compact);
}

void SocketIOClient::tryAutoRegister()
{
    // 如果已经设置了用户名和机器码，自动注册
    if (!m_username.isEmpty() && !m_machineId.isEmpty()) {
        qInfo() << "[SocketIOClient] ✓ 用户名和机器码已设置，自动发送register事件";
        QJsonObject registerData;
        registerData["username"] = m_username;
        registerData["machineId"] = m_machineId;
        emitEvent("register", registerData);
    } else {
        qInfo() << "[SocketIOClient] 用户名或机器码未设置，跳过自动注册";
    }
}
