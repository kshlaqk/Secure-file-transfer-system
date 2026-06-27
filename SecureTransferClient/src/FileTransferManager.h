#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QMap>
#include <QMutex>
#include <QDateTime>

// 任务信息结构
struct TransferTaskInfo {
    QString taskId;           // 任务ID
    QString fileName;         // 文件名
    QString targetId;         // 目标PC编号
    QString localPath;        // 本地文件路径（用于继续传输）
    qint64 totalBytes;        // 总字节数
    qint64 bytesSent;         // 已发送字节数
    QDateTime startTime;      // 开始时间
    bool isCancelled;         // 是否已取消
    bool isPaused;            // 是否已暂停
};

/**
 * @brief 文件传输管理类
 * 
 * 负责管理文件的上传和下载任务
 */
class FileTransferManager : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferManager(QObject *parent = nullptr);
    
    // 传输操作
    QString sendFile(const QString& localPath, const QString& targetId);
    QString resumeTask(const QString& taskId);  // 继续传输
    QString getLastTaskId() const { return m_lastTaskId; }
    
    // 下载操作
    void downloadFile(const QString& fileId, const QString& savePath);  // 下载文件
    void confirmDownload(const QString& fileId);  // 确认下载完成并通知服务器删除文件
    
    // 任务控制
    void pauseTask(const QString& taskId);      // 暂停（发送暂停请求）
    void cancelTask(const QString& taskId);     // 取消（发送取消请求）
    void deleteTask(const QString& taskId);     // 删除（发送删除请求并清理任务）
    
    // 获取任务信息
    bool hasTask(const QString& taskId) const;
    TransferTaskInfo getTaskInfo(const QString& taskId) const;

signals:
    void transferAdded(const QString& fileName, const QString& targetId);
    void transferProgress(const QString& taskId, qint64 bytesSent, qint64 totalBytes);
    void transferCompleted(const QString& taskId, bool success);
    void transferError(const QString& taskId, const QString& error);
    void transferDeleted(const QString& taskId);  // 任务已删除信号
    
    // 下载相关信号
    void downloadProgress(const QString& fileId, qint64 bytesReceived, qint64 totalBytes);
    void downloadCompleted(const QString& fileId, const QString& savePath, bool success);
    void downloadError(const QString& fileId, const QString& error);

private:
    QString generateTaskId();
    void sendFileInBackground(const QString& taskId, const QString& localPath, const QString& targetId);
    void sendFileFromOffset(const QString& taskId, const QString& localPath, const QString& targetId, qint64 startOffset);
    void sendCancelRequest(const QString& targetId, const QString& fileName, qint64 bytesSent, const QDateTime& cancelTime);
    void sendResumeRequest(const QString& targetId, const QString& fileName, qint64 resumeOffset, const QDateTime& resumeTime);
    void sendDeleteRequest(const QString& targetId, const QString& fileName, qint64 deleteOffset, const QDateTime& deleteTime);
    void downloadFileInBackground(const QString& fileId, const QString& savePath);  // 后台下载文件
    QString formatTimestamp(const QDateTime& dateTime);  // 格式化时间戳为"YYYYMMDDHHmm"格式
    
    QString m_lastTaskId;
    mutable QMutex m_mutex;  // mutable 允许在 const 成员函数中使用
    int m_taskCounter;
    
    // 任务管理
    QMap<QString, TransferTaskInfo> m_tasks;  // 任务ID -> 任务信息
    QString m_serverUrl;  // 服务器URL
};

#endif // FILETRANSFERMANAGER_H
