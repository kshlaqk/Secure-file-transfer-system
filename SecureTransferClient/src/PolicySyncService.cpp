#include "PolicySyncService.h"
#include <QJsonObject>
#include <QDebug>

PolicySyncService::PolicySyncService(QObject *parent)
    : QObject(parent)
    , m_driverComm(new DriverCommunicator(this))
    , m_pollInterval(5000)
    , m_autoRetry(true)
    , m_maxRetries(3)
    , m_serviceRunning(false)
{
    // 创建策略同步线程
    m_syncThread = new PolicySyncThread(this);
    
    // 连接信号
    connect(m_syncThread, &PolicySyncThread::policyReceived,
            this, &PolicySyncService::onPolicyReceived);
    connect(m_syncThread, &PolicySyncThread::error,
            this, &PolicySyncService::onPolicySyncError);
    connect(m_syncThread, &PolicySyncThread::webServiceConnected,
            this, &PolicySyncService::webServiceConnected);
    connect(m_syncThread, &PolicySyncThread::webServiceDisconnected,
            this, &PolicySyncService::webServiceDisconnected);
    connect(m_driverComm, &DriverCommunicator::error,
            this, &PolicySyncService::onDriverError);
    connect(m_driverComm, &DriverCommunicator::connected,
            this, &PolicySyncService::driverConnected);
    connect(m_driverComm, &DriverCommunicator::disconnected,
            this, &PolicySyncService::driverDisconnected);
}

PolicySyncService::~PolicySyncService()
{
    stopService();
}

bool PolicySyncService::startService(const QString& webServiceUrl, int pollInterval)
{
    if (m_serviceRunning) {
        qWarning() << "Policy sync service is already running";
        return false;
    }
    
    // 1. 首先连接到驱动
    if (!m_driverComm->isConnected()) {
        if (!m_driverComm->connectToDriver()) {
            emit error("无法连接到驱动，请确保驱动已加载");
            return false;
        }
    }
    
    // 2. 配置Web服务URL和轮询间隔
    m_webServiceUrl = webServiceUrl;
    m_pollInterval = pollInterval;
    m_syncThread->setWebServiceUrl(webServiceUrl);
    m_syncThread->setPollInterval(pollInterval);
    
    // 3. 启动策略同步线程
    m_syncThread->start();
    
    m_serviceRunning = true;
    emit serviceStarted();
    
    qInfo() << "Policy sync service started, polling URL:" << webServiceUrl;
    return true;
}

void PolicySyncService::stopService()
{
    if (!m_serviceRunning) {
        return;
    }
    
    // 停止策略同步线程
    if (m_syncThread->isRunning()) {
        m_syncThread->requestStop();
        m_syncThread->wait(5000);  // 等待最多5秒
        if (m_syncThread->isRunning()) {
            m_syncThread->terminate();
            m_syncThread->wait();
        }
    }
    
    m_serviceRunning = false;
    emit serviceStopped();
    
    qInfo() << "Policy sync service stopped";
}

bool PolicySyncService::isRunning() const
{
    return m_serviceRunning;
}

void PolicySyncService::setWebServiceUrl(const QString& url)
{
    m_webServiceUrl = url;
    if (m_syncThread) {
        m_syncThread->setWebServiceUrl(url);
    }
}

void PolicySyncService::setPollInterval(int milliseconds)
{
    m_pollInterval = milliseconds;
    if (m_syncThread) {
        m_syncThread->setPollInterval(milliseconds);
    }
}

void PolicySyncService::setApiEndpoint(const QString& endpoint)
{
    if (m_syncThread) {
        m_syncThread->setApiEndpoint(endpoint);
    }
}

void PolicySyncService::setAuthenticationToken(const QString& token)
{
    if (m_syncThread) {
        m_syncThread->setAuthenticationToken(token);
    }
}

void PolicySyncService::setAutoRetry(bool enabled, int maxRetries)
{
    m_autoRetry = enabled;
    m_maxRetries = maxRetries;
}

QJsonObject PolicySyncService::getCurrentPolicy() const
{
    return m_currentPolicy;
}

QString PolicySyncService::getCurrentPolicyVersion() const
{
    return m_currentPolicyVersion;
}

bool PolicySyncService::isConnectedToDriver() const
{
    return m_driverComm ? m_driverComm->isConnected() : false;
}

bool PolicySyncService::isConnectedToWebService() const
{
    return m_syncThread ? m_syncThread->isRunning() : false;
}

void PolicySyncService::onPolicyReceived(const QJsonObject& policyData)
{
    qInfo() << "Policy update received from web service";
    
    // 检查策略版本是否变更
    QString newVersion = policyData.value("version").toString();
    if (newVersion == m_currentPolicyVersion) {
        qDebug() << "Policy version unchanged, skipping update";
        return;
    }
    
    // 应用策略到驱动
    if (applyPolicyToDriver(policyData)) {
        m_currentPolicy = policyData;
        m_currentPolicyVersion = newVersion;
        emit policyUpdated(newVersion);
        qInfo() << "Policy updated successfully, version:" << newVersion;
    } else {
        QString error = "Failed to apply policy to driver";
        emit policyUpdateFailed(error);
        qWarning() << error;
    }
}

bool PolicySyncService::applyPolicyToDriver(const QJsonObject& policyData)
{
    // 解析策略数据
    QString extensions = policyData.value("protectedExtensions").toString();
    bool enableEncryption = policyData.value("enableEncryption").toBool(false);
    
    // 更新驱动策略
    return m_driverComm->updatePolicy(extensions, enableEncryption);
}

void PolicySyncService::onPolicySyncError(const QString& error)
{
    qWarning() << "Policy sync error:" << error;
    emit this->error("策略同步错误: " + error);
}

void PolicySyncService::onDriverError(const QString& error)
{
    qWarning() << "Driver communication error:" << error;
    emit this->error("驱动通信错误: " + error);
}




