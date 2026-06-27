#include "WebServiceClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkRequest>
#include <QDebug>
#include <QCoreApplication>

WebServiceClient::WebServiceClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_timeout(10000)  // 默认10秒超时
{
}

WebServiceClient::~WebServiceClient()
{
}

bool WebServiceClient::getRequest(const QString& url, QByteArray& response)
{
    QUrl requestUrl(url);
    if (!requestUrl.isValid()) {
        m_lastError = "Invalid URL: " + url;
        return false;
    }

    QNetworkRequest request(requestUrl);
    setAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "SecureTransferClient/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    // 同步等待响应（在单独的线程中运行）
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QTimer::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QNetworkReply::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(m_timeout);
    loop.exec();

    bool success = false;
    if (timer.isActive()) {
        // 正常完成
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            response = reply->readAll();
            success = true;
            emit requestCompleted(response);
        } else {
            m_lastError = reply->errorString();
            qWarning() << "Network request error:" << m_lastError;
            emit requestError(m_lastError);
        }
    } else {
        // 超时
        m_lastError = "Request timeout";
        reply->abort();
        emit requestError(m_lastError);
    }

    reply->deleteLater();
    return success;
}

bool WebServiceClient::postRequest(const QString& url, const QJsonObject& data, QByteArray& response)
{
    QUrl requestUrl(url);
    if (!requestUrl.isValid()) {
        m_lastError = "Invalid URL: " + url;
        return false;
    }

    QNetworkRequest request(requestUrl);
    setAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "SecureTransferClient/1.0");

    QJsonDocument doc(data);
    QByteArray postData = doc.toJson();

    QNetworkReply* reply = m_networkManager->post(request, postData);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QTimer::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QNetworkReply::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(m_timeout);
    loop.exec();

    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            response = reply->readAll();
            success = true;
            emit requestCompleted(response);
        } else {
            m_lastError = reply->errorString();
            emit requestError(m_lastError);
        }
    } else {
        m_lastError = "Request timeout";
        reply->abort();
        emit requestError(m_lastError);
    }

    reply->deleteLater();
    return success;
}

void WebServiceClient::setAuthToken(const QString& token)
{
    m_authToken = token;
}

void WebServiceClient::setTimeout(int milliseconds)
{
    m_timeout = milliseconds;
}

QString WebServiceClient::getLastError() const
{
    return m_lastError;
}

void WebServiceClient::setAuthHeader(QNetworkRequest& request)
{
    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", 
            QString("Bearer %1").arg(m_authToken).toUtf8());
    }
}

void WebServiceClient::onRequestFinished(QNetworkReply* reply)
{
    // 异步处理（如果需要）
    Q_UNUSED(reply)
}




