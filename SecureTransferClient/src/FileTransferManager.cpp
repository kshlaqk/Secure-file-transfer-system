#include "FileTransferManager.h"
#include "WebServiceClient.h"
#include <QUuid>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>
#include <QMutexLocker>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>

FileTransferManager::FileTransferManager(QObject *parent)
    : QObject(parent)
    , m_taskCounter(0)
{
    // 从设置获取服务器URL
    QSettings settings;
    m_serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
}

QString FileTransferManager::generateTaskId()
{
    QMutexLocker locker(&m_mutex);
    m_taskCounter++;
    m_lastTaskId = QString("task_%1_%2")
                  .arg(m_taskCounter)
                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    return m_lastTaskId;
}

QString FileTransferManager::sendFile(const QString& localPath, const QString& targetId)
{
    QString taskId = generateTaskId();
    QFileInfo fileInfo(localPath);
    QString fileName = fileInfo.fileName();
    
    if (!fileInfo.exists()) {
        emit transferError(taskId, "文件不存在");
        return taskId;
    }
    
    // 初始化任务信息
    TransferTaskInfo taskInfo;
    taskInfo.taskId = taskId;
    taskInfo.fileName = fileName;
    taskInfo.targetId = targetId;
    taskInfo.localPath = localPath;  // 保存本地路径，用于继续传输
    taskInfo.totalBytes = fileInfo.size();
    taskInfo.bytesSent = 0;
    taskInfo.startTime = QDateTime::currentDateTime();
    taskInfo.isCancelled = false;
    taskInfo.isPaused = false;
    
    {
        QMutexLocker locker(&m_mutex);
        m_tasks[taskId] = taskInfo;
    }
    
    // 在后台线程中发送文件
    QThread* thread = QThread::create([this, taskId, localPath, targetId]() {
        sendFileInBackground(taskId, localPath, targetId);
    });
    
    thread->start();
    emit transferAdded(fileName, targetId);
    
    return taskId;
}

void FileTransferManager::sendFileInBackground(const QString& taskId, 
                                                const QString& localPath, 
                                                const QString& targetId)
{
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit transferError(taskId, "无法打开文件: " + file.errorString());
        
        // 清理任务
        QMutexLocker locker(&m_mutex);
        m_tasks.remove(taskId);
        return;
    }
    
    QFileInfo fileInfo(localPath);
    qint64 fileSize = fileInfo.size();
    
    // 读取文件数据（实际实现中可能需要分块读取以避免大文件占用过多内存）
    QByteArray fileData = file.readAll();
    file.close();
    
    // 检查是否已取消
    {
        QMutexLocker locker(&m_mutex);
        if (m_tasks.contains(taskId) && m_tasks[taskId].isCancelled) {
            // 发送取消请求
            TransferTaskInfo& taskInfo = m_tasks[taskId];
            sendCancelRequest(taskInfo.targetId, taskInfo.fileName, 0, QDateTime::currentDateTime());
            
            // 清理任务
            m_tasks.remove(taskId);
            emit transferError(taskId, "传输已取消");
            return;
        }
    }
    
    // 获取当前登录的用户名（作为发送者标识）
    QSettings settings;
    QString senderId = settings.value("username", "unknown").toString();
    
    // 构建JSON请求（包含协议头信息）
    QJsonObject request;
    request["magic"] = 0x46545251;  // "FTQ"协议标识
    request["version"] = 1;
    request["senderId"] = senderId;  // 发送者用户名（名字+工号）
    request["targetId"] = targetId;  // 目标用户名（名字+工号）
    request["fileName"] = fileInfo.fileName();
    request["fileSize"] = static_cast<qint64>(fileSize);
    
    // 将文件数据编码为Base64
    QByteArray base64Data = fileData.toBase64();
    request["fileData"] = QString::fromLatin1(base64Data);
    
    // 发送到服务器
    QString apiUrl = m_serverUrl + "/api/file/transfer";
    WebServiceClient webClient;
    QByteArray response;
    
    // 发送进度更新（发送前）
    emit transferProgress(taskId, 0, fileSize);
    
    if (webClient.postRequest(apiUrl, request, response)) {
        // 检查是否在传输过程中被取消
        bool wasCancelled = false;
        qint64 bytesSent = 0;
        {
            QMutexLocker locker(&m_mutex);
            if (m_tasks.contains(taskId)) {
                wasCancelled = m_tasks[taskId].isCancelled;
                bytesSent = m_tasks[taskId].bytesSent;
            }
        }
        
        if (wasCancelled) {
            // 发送取消请求
            TransferTaskInfo taskInfo;
            {
                QMutexLocker locker(&m_mutex);
                if (m_tasks.contains(taskId)) {
                    taskInfo = m_tasks[taskId];
                }
            }
            sendCancelRequest(taskInfo.targetId, taskInfo.fileName, fileSize, QDateTime::currentDateTime());
            
            // 清理任务
            QMutexLocker locker(&m_mutex);
            m_tasks.remove(taskId);
            emit transferError(taskId, "传输已取消");
            return;
        }
        
        // 更新已发送字节数
        {
            QMutexLocker locker(&m_mutex);
            if (m_tasks.contains(taskId)) {
                m_tasks[taskId].bytesSent = fileSize;
            }
        }
        
        // 解析响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                emit transferProgress(taskId, fileSize, fileSize);
                emit transferCompleted(taskId, true);
            } else {
                emit transferError(taskId, result["error"].toString("服务器返回错误"));
            }
        } else {
            emit transferError(taskId, "服务器响应格式错误");
        }
    } else {
        emit transferError(taskId, "网络请求失败: " + webClient.getLastError());
    }
    
    // 清理任务（如果还没被清理）
    {
        QMutexLocker locker(&m_mutex);
        m_tasks.remove(taskId);
    }
}

void FileTransferManager::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        qWarning() << "Cancel task failed: task not found:" << taskId;
        return;
    }
    
    TransferTaskInfo& taskInfo = m_tasks[taskId];
    
    // 如果任务已经完成或已取消，直接返回
    if (taskInfo.isCancelled) {
        return;
    }
    
    // 标记任务为已取消
    taskInfo.isCancelled = true;
    qint64 bytesSent = taskInfo.bytesSent;
    QDateTime cancelTime = QDateTime::currentDateTime();
    QString targetId = taskInfo.targetId;
    QString fileName = taskInfo.fileName;
    
    // 释放锁，然后发送取消请求（避免在持有锁的情况下进行网络操作）
    locker.unlock();
    
    // 发送取消请求到服务器
    sendCancelRequest(targetId, fileName, bytesSent, cancelTime);
    
    qInfo() << "Task cancelled:" << taskId 
            << "Bytes sent:" << bytesSent 
            << "Cancel time:" << formatTimestamp(cancelTime);
}

void FileTransferManager::sendCancelRequest(const QString& targetId, 
                                             const QString& fileName, 
                                             qint64 bytesSent, 
                                             const QDateTime& cancelTime)
{
    // 构建取消请求
    QJsonObject cancelRequest;
    cancelRequest["magic"] = 0x4654434E;  // "FTCN" - File Transfer Cancel
    cancelRequest["version"] = 1;
    cancelRequest["targetId"] = targetId;  // 目标PC编号
    cancelRequest["fileName"] = fileName;  // 文件名
    cancelRequest["cancelTimestamp"] = formatTimestamp(cancelTime);  // 时间戳：YYYYMMDDHHmm
    cancelRequest["interruptOffset"] = static_cast<qint64>(bytesSent);  // 中断偏移（已传输字节数）
    
    // 发送到服务器
    QString apiUrl = m_serverUrl + "/api/file/cancel";
    WebServiceClient webClient;
    QByteArray response;
    
    if (webClient.postRequest(apiUrl, cancelRequest, response)) {
        qInfo() << "Cancel request sent successfully:" 
                << "targetId=" << targetId 
                << "fileName=" << fileName
                << "offset=" << bytesSent;
        
        // 可选：解析服务器响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (!result["success"].toBool()) {
                qWarning() << "Server returned error for cancel request:" 
                          << result["error"].toString();
            }
        }
    } else {
        qWarning() << "Failed to send cancel request:" << webClient.getLastError();
    }
}

QString FileTransferManager::resumeTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        qWarning() << "Resume task failed: task not found:" << taskId;
        emit transferError(taskId, "任务不存在，无法继续传输");
        return QString();
    }
    
    TransferTaskInfo& taskInfo = m_tasks[taskId];
    
    // 如果任务已经完成，无法继续
    if (taskInfo.bytesSent >= taskInfo.totalBytes) {
        emit transferError(taskId, "文件已传输完成，无需继续");
        return taskId;
    }
    
    // 获取继续传输信息
    QString targetId = taskInfo.targetId;
    QString fileName = taskInfo.fileName;
    QString localPath = taskInfo.localPath;
    qint64 resumeOffset = taskInfo.bytesSent;  // 从上次中断的位置继续
    QDateTime resumeTime = QDateTime::currentDateTime();
    
    // 重置取消/暂停标志
    taskInfo.isCancelled = false;
    taskInfo.isPaused = false;
    
    // 释放锁，然后发送继续传输请求
    locker.unlock();
    
    // 发送继续传输请求到服务器
    sendResumeRequest(targetId, fileName, resumeOffset, resumeTime);
    
    qInfo() << "Resuming task:" << taskId 
            << "targetId:" << targetId
            << "fileName:" << fileName
            << "resumeOffset:" << resumeOffset
            << "resumeTime:" << formatTimestamp(resumeTime);
    
    // 继续传输文件（从指定偏移量开始）
    sendFileFromOffset(taskId, localPath, targetId, resumeOffset);
    
    return taskId;
}

void FileTransferManager::sendResumeRequest(const QString& targetId, 
                                              const QString& fileName, 
                                              qint64 resumeOffset, 
                                              const QDateTime& resumeTime)
{
    // 构建继续传输请求
    QJsonObject resumeRequest;
    resumeRequest["magic"] = 0x46545245;  // "FTRE" - File Transfer Resume
    resumeRequest["version"] = 1;
    resumeRequest["targetId"] = targetId;  // 目标PC编号
    resumeRequest["fileName"] = fileName;  // 文件名
    resumeRequest["resumeTimestamp"] = formatTimestamp(resumeTime);  // 时间戳：YYYYMMDDHHmm
    resumeRequest["resumeOffset"] = static_cast<qint64>(resumeOffset);  // 继续传输的起始偏移
    
    // 发送到服务器
    QString apiUrl = m_serverUrl + "/api/file/resume";
    WebServiceClient webClient;
    QByteArray response;
    
    if (webClient.postRequest(apiUrl, resumeRequest, response)) {
        qInfo() << "Resume request sent successfully:" 
                << "targetId=" << targetId 
                << "fileName=" << fileName
                << "offset=" << resumeOffset;
        
        // 可选：解析服务器响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (!result["success"].toBool()) {
                qWarning() << "Server returned error for resume request:" 
                          << result["error"].toString();
            }
        }
    } else {
        qWarning() << "Failed to send resume request:" << webClient.getLastError();
    }
}

void FileTransferManager::sendFileFromOffset(const QString& taskId, 
                                              const QString& localPath, 
                                              const QString& targetId, 
                                              qint64 startOffset)
{
    // 在后台线程中继续传输文件（从指定偏移量开始）
    QThread* thread = QThread::create([this, taskId, localPath, targetId, startOffset]() {
        QFile file(localPath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit transferError(taskId, "无法打开文件: " + file.errorString());
            return;
        }
        
        QFileInfo fileInfo(localPath);
        qint64 fileSize = fileInfo.size();
        
        // 如果起始偏移量大于等于文件大小，说明文件已经传输完成
        if (startOffset >= fileSize) {
            emit transferError(taskId, "文件已传输完成，无需继续");
            file.close();
            return;
        }
        
        // 移动到指定偏移量位置
        if (!file.seek(startOffset)) {
            emit transferError(taskId, "无法定位到文件偏移量位置");
            file.close();
            return;
        }
        
        // 读取剩余数据
        QByteArray remainingData = file.readAll();
        file.close();
        
        qint64 remainingSize = remainingData.size();
        
        // 检查是否已取消
        {
            QMutexLocker locker(&m_mutex);
            if (m_tasks.contains(taskId) && m_tasks[taskId].isCancelled) {
                sendCancelRequest(targetId, fileInfo.fileName(), startOffset + remainingSize, QDateTime::currentDateTime());
                m_tasks.remove(taskId);
                emit transferError(taskId, "传输已取消");
                return;
            }
        }
        
        // 构建JSON请求（仅包含剩余数据）
        QJsonObject request;
        request["magic"] = 0x46545251;  // "FTQ"协议标识
        request["version"] = 1;
        request["targetId"] = targetId;
        request["fileName"] = fileInfo.fileName();
        request["fileSize"] = static_cast<qint64>(fileSize);
        request["startOffset"] = static_cast<qint64>(startOffset);  // 起始偏移量
        request["chunkData"] = QString::fromLatin1(remainingData.toBase64());  // 剩余数据（Base64编码）
        
        // 发送到服务器
        QString apiUrl = m_serverUrl + "/api/file/transfer";
        WebServiceClient webClient;
        QByteArray response;
        
        // 发送进度更新
        emit transferProgress(taskId, startOffset, fileSize);
        
        if (webClient.postRequest(apiUrl, request, response)) {
            // 检查是否被取消
            bool wasCancelled = false;
            {
                QMutexLocker locker(&m_mutex);
                if (m_tasks.contains(taskId) && m_tasks[taskId].isCancelled) {
                    wasCancelled = true;
                }
            }
            
            if (wasCancelled) {
                sendCancelRequest(targetId, fileInfo.fileName(), fileSize, QDateTime::currentDateTime());
                QMutexLocker locker(&m_mutex);
                m_tasks.remove(taskId);
                emit transferError(taskId, "传输已取消");
                return;
            }
            
            // 更新已发送字节数
            {
                QMutexLocker locker(&m_mutex);
                if (m_tasks.contains(taskId)) {
                    m_tasks[taskId].bytesSent = fileSize;
                }
            }
            
            // 解析响应
            QJsonDocument doc = QJsonDocument::fromJson(response);
            if (doc.isObject()) {
                QJsonObject result = doc.object();
                if (result["success"].toBool()) {
                    emit transferProgress(taskId, fileSize, fileSize);
                    emit transferCompleted(taskId, true);
                } else {
                    emit transferError(taskId, result["error"].toString("服务器返回错误"));
                }
            } else {
                emit transferError(taskId, "服务器响应格式错误");
            }
        } else {
            emit transferError(taskId, "网络请求失败: " + webClient.getLastError());
        }
        
        // 清理任务
        {
            QMutexLocker locker(&m_mutex);
            m_tasks.remove(taskId);
        }
    });
    
    thread->start();
}

void FileTransferManager::deleteTask(const QString& taskId)
{
    // 获取任务信息
    QString targetId;
    QString fileName;
    qint64 deleteOffset = 0;
    QDateTime deleteTime = QDateTime::currentDateTime();
    
    {
        QMutexLocker locker(&m_mutex);
        
        if (!m_tasks.contains(taskId)) {
            qWarning() << "Delete task failed: task not found:" << taskId;
            return;
        }
        
        TransferTaskInfo& taskInfo = m_tasks[taskId];
        targetId = taskInfo.targetId;
        fileName = taskInfo.fileName;
        deleteOffset = taskInfo.bytesSent;  // 获取当前已传输的字节数
        
        // 标记任务为已取消（如果正在传输，需要停止）
        taskInfo.isCancelled = true;
        
        // 释放锁，然后发送删除请求
        locker.unlock();
        
        // 发送删除请求到服务器
        sendDeleteRequest(targetId, fileName, deleteOffset, deleteTime);
        
        qInfo() << "Task deleted:" << taskId 
                << "targetId:" << targetId
                << "fileName:" << fileName
                << "deleteOffset:" << deleteOffset
                << "deleteTime:" << formatTimestamp(deleteTime);
        
        // 重新获取锁，清理任务信息
        locker.relock();
        m_tasks.remove(taskId);
    }
    
    // 发送任务已删除信号
    emit transferDeleted(taskId);
}

void FileTransferManager::sendDeleteRequest(const QString& targetId, 
                                               const QString& fileName, 
                                               qint64 deleteOffset, 
                                               const QDateTime& deleteTime)
{
    // 构建删除请求
    QJsonObject deleteRequest;
    deleteRequest["magic"] = 0x46544445;  // "FTDE" - File Transfer Delete
    deleteRequest["version"] = 1;
    deleteRequest["targetId"] = targetId;  // 目标PC编号
    deleteRequest["fileName"] = fileName;  // 文件名
    deleteRequest["deleteTimestamp"] = formatTimestamp(deleteTime);  // 时间戳：YYYYMMDDHHmm
    deleteRequest["deleteOffset"] = static_cast<qint64>(deleteOffset);  // 删除时的传输偏移（已传输字节数）
    
    // 发送到服务器
    QString apiUrl = m_serverUrl + "/api/file/delete";
    WebServiceClient webClient;
    QByteArray response;
    
    if (webClient.postRequest(apiUrl, deleteRequest, response)) {
        qInfo() << "Delete request sent successfully:" 
                << "targetId=" << targetId 
                << "fileName=" << fileName
                << "offset=" << deleteOffset;
        
        // 可选：解析服务器响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (!result["success"].toBool()) {
                qWarning() << "Server returned error for delete request:" 
                          << result["error"].toString();
            }
        }
    } else {
        qWarning() << "Failed to send delete request:" << webClient.getLastError();
    }
}

void FileTransferManager::pauseTask(const QString& taskId)
{
    Q_UNUSED(taskId)
    // 暂停功能可以根据需要实现
    qInfo() << "Pause task:" << taskId << "(Not implemented yet)";
}

bool FileTransferManager::hasTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.contains(taskId);
}

TransferTaskInfo FileTransferManager::getTaskInfo(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    if (m_tasks.contains(taskId)) {
        return m_tasks[taskId];
    }
    return TransferTaskInfo();  // 返回空的默认结构
}

QString FileTransferManager::formatTimestamp(const QDateTime& dateTime)
{
    // 格式化为 "YYYYMMDDHHmm"（年月日时分）
    // 例如：202501011430 表示 2025年1月1日 14:30
    return dateTime.toString("yyyyMMddHHmm");
}

void FileTransferManager::downloadFile(const QString& fileId, const QString& savePath)
{
    // 在后台线程中执行下载
    QThread* thread = QThread::create([this, fileId, savePath]() {
        downloadFileInBackground(fileId, savePath);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void FileTransferManager::downloadFileInBackground(const QString& fileId, const QString& savePath)
{
    QSettings settings;
    QString serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
    QString username = settings.value("username", "").toString();
    
    // 构建下载URL，包含targetId参数用于权限验证
    QUrl downloadUrl(serverUrl + "/api/files/download/" + fileId);
    QUrlQuery query;
    query.addQueryItem("targetId", username);
    downloadUrl.setQuery(query);
    
    QNetworkAccessManager* networkManager = new QNetworkAccessManager();
    QNetworkRequest request(downloadUrl);
    request.setRawHeader("User-Agent", "SecureTransferClient/1.0");
    
    QNetworkReply* reply = networkManager->get(request);
    
    // 创建临时文件用于写入
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit downloadError(fileId, QString("无法创建文件: %1").arg(savePath));
        reply->deleteLater();
        networkManager->deleteLater();
        return;
    }
    
    // 连接进度信号
    QObject::connect(reply, &QNetworkReply::downloadProgress, 
                     [this, fileId](qint64 bytesReceived, qint64 totalBytes) {
        emit downloadProgress(fileId, bytesReceived, totalBytes);
    });
    
    // 连接数据接收信号
    QObject::connect(reply, &QNetworkReply::readyRead, [&file, reply]() {
        QByteArray data = reply->readAll();
        file.write(data);
    });
    
    // 等待下载完成
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(300000); // 5分钟超时
    
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    timer.start();
    loop.exec();
    
    bool success = false;
    QString errorMsg;
    
    if (timer.isActive()) {
        // 正常完成
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            // 确保所有数据都已写入
            QByteArray remainingData = reply->readAll();
            if (!remainingData.isEmpty()) {
                file.write(remainingData);
            }
            file.flush();
            success = true;
        } else {
            errorMsg = reply->errorString();
        }
    } else {
        // 超时
        errorMsg = "下载超时";
        reply->abort();
    }
    
    file.close();
    
    if (success) {
        emit downloadCompleted(fileId, savePath, true);
    } else {
        // 删除不完整的文件
        if (file.exists()) {
            file.remove();
        }
        emit downloadError(fileId, errorMsg.isEmpty() ? "下载失败" : errorMsg);
    }
    
    reply->deleteLater();
    networkManager->deleteLater();
}

void FileTransferManager::confirmDownload(const QString& fileId)
{
    QSettings settings;
    QString serverUrl = settings.value("webServiceUrl", "http://localhost:8080").toString();
    QString username = settings.value("username", "").toString();
    
    qDebug() << "confirmDownload called - fileId:" << fileId << "username:" << username << "serverUrl:" << serverUrl;
    
    // 构建请求URL
    QString apiUrl = serverUrl + "/api/files/confirm-download";
    
    // 构建请求数据
    QJsonObject request;
    request["fileId"] = fileId;
    request["targetId"] = username;
    
    qDebug() << "Sending confirm-download request to:" << apiUrl;
    qDebug() << "Request data:" << QJsonDocument(request).toJson();
    
    // 发送POST请求
    WebServiceClient client;
    QByteArray response;
    
    if (client.postRequest(apiUrl, request, response)) {
        qDebug() << "Server response:" << response;
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                qDebug() << "文件删除确认成功:" << fileId;
            } else {
                qWarning() << "文件删除确认失败:" << result["error"].toString();
            }
        } else {
            qWarning() << "服务器响应格式错误";
        }
    } else {
        qWarning() << "无法连接到服务器确认下载:" << client.getLastError();
    }
}
