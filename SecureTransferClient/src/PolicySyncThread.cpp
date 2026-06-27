#include "PolicySyncThread.h"
#include "WebServiceClient.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QMutexLocker>

PolicySyncThread::PolicySyncThread(QObject *parent)
    : QThread(parent)
    , m_apiEndpoint("/api/policy/current")
    , m_pollInterval(5000)
    , m_stopRequested(false)
    , m_isStopped(false)
{
}

PolicySyncThread::~PolicySyncThread()
{
    requestStop();
    wait(5000);
    if (isRunning()) {
        terminate();
        wait();
    }
}

void PolicySyncThread::setWebServiceUrl(const QString& url)
{
    QMutexLocker locker(&m_mutex);
    m_webServiceUrl = url;
}

void PolicySyncThread::setPollInterval(int milliseconds)
{
    QMutexLocker locker(&m_mutex);
    m_pollInterval = milliseconds;
}

void PolicySyncThread::setApiEndpoint(const QString& endpoint)
{
    QMutexLocker locker(&m_mutex);
    m_apiEndpoint = endpoint;
}

void PolicySyncThread::setAuthenticationToken(const QString& token)
{
    QMutexLocker locker(&m_mutex);
    m_authToken = token;
}

void PolicySyncThread::requestStop()
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
    m_condition.wakeAll();
}

bool PolicySyncThread::isStopped() const
{
    QMutexLocker locker(&m_mutex);
    return m_isStopped;
}

void PolicySyncThread::run()
{
    qInfo() << "Policy sync thread started";
    
    QString currentPolicyVersion;
    
    {
        QMutexLocker locker(&m_mutex);
        m_stopRequested = false;
        m_isStopped = false;
    }
    
    while (true) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopRequested) {
                break;
            }
        }
        
        // 从Web服务获取策略
        QJsonObject policyData = fetchPolicyFromWebService();
        
        if (!policyData.isEmpty()) {
            QString newVersion = policyData.value("version").toString();
            
            // 检查版本是否变更
            if (newVersion != currentPolicyVersion) {
                currentPolicyVersion = newVersion;
                emit policyReceived(policyData);
            }
            
            emit webServiceConnected();
        } else {
            emit webServiceDisconnected();
        }
        
        // 等待指定间隔或收到停止请求
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopRequested) {
                break;
            }
            m_condition.wait(&m_mutex, m_pollInterval);
        }
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_isStopped = true;
    }
    
    qInfo() << "Policy sync thread stopped";
}

QJsonObject PolicySyncThread::fetchPolicyFromWebService()
{
    QString url, endpoint, token;
    
    {
        QMutexLocker locker(&m_mutex);
        url = m_webServiceUrl;
        endpoint = m_apiEndpoint;
        token = m_authToken;
    }
    
    if (url.isEmpty()) {
        emit error("Web服务URL未配置");
        return QJsonObject();
    }
    
    // 构建完整URL
    QString fullUrl = url;
    if (!fullUrl.endsWith("/") && !endpoint.startsWith("/")) {
        fullUrl += "/";
    }
    fullUrl += endpoint;
    
    // 使用WebServiceClient获取策略
    WebServiceClient client;
    client.setAuthToken(token);
    
    QByteArray response;
    if (client.getRequest(fullUrl, response)) {
        QJsonObject policyData;
        if (parsePolicyResponse(response, policyData)) {
            return policyData;
        } else {
            emit error("无法解析策略响应");
        }
    } else {
        emit error("无法连接到Web服务: " + client.getLastError());
    }
    
    return QJsonObject();
}

bool PolicySyncThread::parsePolicyResponse(const QByteArray& response, QJsonObject& policyData)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }
    
    if (doc.isObject()) {
        policyData = doc.object();
        
        // 验证必需字段
        if (policyData.contains("version") && 
            policyData.contains("protectedExtensions")) {
            return true;
        } else {
            qWarning() << "Policy data missing required fields";
        }
    }
    
    return false;
}




