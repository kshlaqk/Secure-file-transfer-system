#ifndef POLICYSYNCTHREAD_H
#define POLICYSYNCTHREAD_H

#include <QThread>
#include <QJsonObject>
#include <QString>
#include <QMutex>
#include <QWaitCondition>

/**
 * @brief 策略同步线程 - 定期从Web服务获取策略更新
 * 
 * 工作流程：
 * 1. 定期轮询Web服务API获取最新策略
 * 2. 比较策略版本，如有更新则发出信号
 */
class PolicySyncThread : public QThread
{
    Q_OBJECT

public:
    explicit PolicySyncThread(QObject *parent = nullptr);
    ~PolicySyncThread();

    // 配置
    void setWebServiceUrl(const QString& url);
    void setPollInterval(int milliseconds);
    void setApiEndpoint(const QString& endpoint);  // 例如: "/api/policy/current"
    void setAuthenticationToken(const QString& token);
    
    // 控制
    void requestStop();
    bool isStopped() const;

signals:
    // 策略更新信号
    void policyReceived(const QJsonObject& policyData);
    
    // 连接状态信号
    void webServiceConnected();
    void webServiceDisconnected();
    
    // 错误信号
    void error(const QString& errorString);

protected:
    void run() override;

private:
    QJsonObject fetchPolicyFromWebService();
    bool parsePolicyResponse(const QByteArray& response, QJsonObject& policyData);
    
    QString m_webServiceUrl;
    QString m_apiEndpoint;
    QString m_authToken;
    int m_pollInterval;
    
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_stopRequested;
    bool m_isStopped;
};

#endif // POLICYSYNCTHREAD_H


