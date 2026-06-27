#ifndef POLICYSYNCSERVICE_H
#define POLICYSYNCSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "PolicySyncThread.h"
#include "DriverCommunicator.h"

/**
 * @brief 策略同步服务 - 管理策略同步线程和驱动通信
 * 
 * 负责：
 * 1. 启动/停止策略同步线程
 * 2. 处理策略更新请求
 * 3. 与驱动通信更新策略
 * 4. 提供策略状态查询
 */
class PolicySyncService : public QObject
{
    Q_OBJECT

public:
    explicit PolicySyncService(QObject *parent = nullptr);
    ~PolicySyncService();

    // 服务控制
    bool startService(const QString& webServiceUrl, int pollInterval = 5000);
    void stopService();
    bool isRunning() const;

    // 配置
    void setWebServiceUrl(const QString& url);
    void setPollInterval(int milliseconds);
    void setApiEndpoint(const QString& endpoint);
    void setAuthenticationToken(const QString& token);
    void setAutoRetry(bool enabled, int maxRetries = 3);

    // 策略查询
    QJsonObject getCurrentPolicy() const;
    QString getCurrentPolicyVersion() const;
    bool isConnectedToDriver() const;
    bool isConnectedToWebService() const;

signals:
    // 状态信号
    void serviceStarted();
    void serviceStopped();
    void driverConnected();
    void driverDisconnected();
    void webServiceConnected();
    void webServiceDisconnected();
    
    // 策略更新信号
    void policyUpdated(const QString& policyVersion);
    void policyUpdateFailed(const QString& error);
    
    // 错误信号
    void error(const QString& errorString);

private slots:
    void onPolicyReceived(const QJsonObject& policyData);
    void onPolicySyncError(const QString& error);
    void onDriverError(const QString& error);

private:
    bool applyPolicyToDriver(const QJsonObject& policyData);
    
    PolicySyncThread* m_syncThread;
    DriverCommunicator* m_driverComm;
    
    QString m_webServiceUrl;
    int m_pollInterval;
    bool m_autoRetry;
    int m_maxRetries;
    
    QJsonObject m_currentPolicy;
    QString m_currentPolicyVersion;
    bool m_serviceRunning;
};

#endif // POLICYSYNCSERVICE_H




