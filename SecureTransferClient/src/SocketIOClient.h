#ifndef SOCKETIOCLIENT_H
#define SOCKETIOCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>

class SocketIOClient : public QObject
{
    Q_OBJECT

public:
    explicit SocketIOClient(QObject *parent = nullptr);
    ~SocketIOClient();

    // 连接管理
    void connectToServer(const QString& serverUrl);
    void disconnectFromServer();
    bool isConnected() const;

    // 注册客户端
    void registerClient(const QString& username, const QString& machineId);

    // 发送事件
    void emitEvent(const QString& event, const QJsonObject& data = QJsonObject());

signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);
    void eventReceived(const QString& event, const QJsonObject& data);

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextMessageReceived(const QString& message);
    void onHeartbeatTimeout();

private:
    void sendHeartbeat();
    void parseSocketIOMessage(const QString& message);
    QString encodeSocketIOMessage(const QString& event, const QJsonObject& data);
    void tryAutoRegister();  // 尝试自动注册
    
    QWebSocket* m_webSocket;
    QTimer* m_heartbeatTimer;
    QString m_serverUrl;
    QString m_username;
    QString m_machineId;
    bool m_connected;
    bool m_handshakeCompleted;  // Socket.io握手是否完成
    int m_pingInterval;  // 心跳间隔（毫秒）
};

#endif // SOCKETIOCLIENT_H
