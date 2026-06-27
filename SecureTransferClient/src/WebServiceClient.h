#ifndef WEBSERVICECLIENT_H
#define WEBSERVICECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QString>
#include <QJsonObject>
#include <QByteArray>

/**
 * @brief Web服务客户端 - 处理与Web后台的HTTP通信
 */
class WebServiceClient : public QObject
{
    Q_OBJECT

public:
    explicit WebServiceClient(QObject *parent = nullptr);
    ~WebServiceClient();

    // HTTP请求
    bool getRequest(const QString& url, QByteArray& response);
    bool postRequest(const QString& url, const QJsonObject& data, QByteArray& response);
    
    // 配置
    void setAuthToken(const QString& token);
    void setTimeout(int milliseconds);
    
    // 错误处理
    QString getLastError() const;

signals:
    void requestCompleted(const QByteArray& response);
    void requestError(const QString& error);

private slots:
    void onRequestFinished(QNetworkReply* reply);

private:
    void setAuthHeader(QNetworkRequest& request);
    
    QNetworkAccessManager* m_networkManager;
    QString m_authToken;
    int m_timeout;
    QString m_lastError;
};

#endif // WEBSERVICECLIENT_H




