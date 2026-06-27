#include "LoginDialog.h"
#include "WebServiceClient.h"
#include "MachineIdHelper.h"
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QFormLayout>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , m_isRegistered(false)
{
    setWindowTitle("登录/注册");
    setModal(true);
    setMinimumWidth(400);
    
    // 获取机器码
    m_machineId = MachineIdHelper::getMachineId();
    
    setupUI();
    
    // 加载保存的配置
    QSettings settings;
    m_usernameEdit->setText(settings.value("username", "").toString());
    m_serverUrlEdit->setText(settings.value("webServiceUrl", "http://192.168.238.20:8080").toString());
}

void LoginDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout();
    
    // 账号输入（名字+工号格式）
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("例如: 李明028");
    formLayout->addRow("账号（名字+工号）:", m_usernameEdit);
    
    // 密码输入
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("请输入密码（至少6位）");
    formLayout->addRow("密码:", m_passwordEdit);
    
    // 服务器URL输入
    m_serverUrlEdit = new QLineEdit();
    m_serverUrlEdit->setPlaceholderText("http://192.168.238.20:8080");
    formLayout->addRow("服务器URL:", m_serverUrlEdit);
    
    // 注册模式复选框
    m_registerModeCheck = new QCheckBox("注册新账号");
    formLayout->addRow("", m_registerModeCheck);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(10);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_registerBtn = new QPushButton("注册");
    m_loginBtn = new QPushButton("登录");
    m_cancelBtn = new QPushButton("取消");
    
    // 初始状态：隐藏注册按钮，显示登录按钮
    m_registerBtn->setVisible(false);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_registerBtn);
    buttonLayout->addWidget(m_loginBtn);
    buttonLayout->addWidget(m_cancelBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_registerModeCheck, &QCheckBox::toggled, this, &LoginDialog::onRegisterModeChanged);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginDialog::onRegister);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void LoginDialog::onRegisterModeChanged(bool checked)
{
    // 切换按钮显示
    m_registerBtn->setVisible(checked);
    m_loginBtn->setVisible(!checked);
}

bool LoginDialog::validateInput(bool isRegister)
{
    if (m_usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "账号不能为空");
        return false;
    }
    
    if (m_passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "密码不能为空");
        return false;
    }
    
    if (m_passwordEdit->text().length() < 6) {
        QMessageBox::warning(this, "输入错误", "密码长度至少6位");
        return false;
    }
    
    if (m_machineId.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取本机机器码，无法继续");
        return false;
    }
    
    if (m_serverUrlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "服务器URL不能为空");
        return false;
    }
    
    return true;
}

void LoginDialog::onRegister()
{
    if (!validateInput(true)) {
        return;
    }
    
    QString serverUrl = m_serverUrlEdit->text().trimmed();
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    QString machineId = m_machineId;
    
    // 发送注册请求
    QString apiUrl = serverUrl + "/api/auth/register";
    QJsonObject request;
    request["username"] = username;
    request["password"] = password;
    request["machineId"] = machineId;  // 发送机器码
    
    WebServiceClient client;
    QByteArray response;
    
    if (client.postRequest(apiUrl, request, response)) {
        // 解析响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                // 保存配置
                QSettings settings;
                settings.setValue("username", username);
                settings.setValue("machineId", machineId);
                settings.setValue("webServiceUrl", serverUrl);
                
                m_isRegistered = true;
                accept();  // 关闭对话框
            } else {
                QMessageBox::warning(this, "注册失败", 
                    result["error"].toString("服务器返回错误"));
            }
        }
    } else {
        QMessageBox::warning(this, "注册失败", 
            "无法连接到服务器: " + client.getLastError());
    }
}

void LoginDialog::onLogin()
{
    if (!validateInput(false)) {
        return;
    }
    
    QString serverUrl = m_serverUrlEdit->text().trimmed();
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    // 发送登录请求
    QString apiUrl = serverUrl + "/api/auth/login";
    QJsonObject request;
    request["username"] = username;
    request["password"] = password;
    
    WebServiceClient client;
    QByteArray response;
    
    if (client.postRequest(apiUrl, request, response)) {
        // 解析响应
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject result = doc.object();
            if (result["success"].toBool()) {
                // 获取用户信息
                QJsonObject user = result["user"].toObject();
                QString machineId = user["machineId"].toString();
                
                // 验证机器码是否匹配
                if (!machineId.isEmpty() && machineId != m_machineId) {
                    QMessageBox::warning(this, "登录失败", 
                        "本机机器码与账号绑定的机器码不匹配！");
                    return;
                }
                
                // 保存配置
                QSettings settings;
                settings.setValue("username", username);
                settings.setValue("machineId", m_machineId);
                settings.setValue("webServiceUrl", serverUrl);
                
                m_isRegistered = true;
                accept();
            } else {
                QMessageBox::warning(this, "登录失败", 
                    result["error"].toString("用户名或密码错误"));
            }
        }
    } else {
        QMessageBox::warning(this, "登录失败", 
            "无法连接到服务器: " + client.getLastError());
    }
}

QString LoginDialog::getUsername() const
{
    return m_usernameEdit->text().trimmed();
}

QString LoginDialog::getMachineId() const
{
    return m_machineId;
}

QString LoginDialog::getMachineIdInternal() const
{
    if (m_machineId.isEmpty()) {
        return MachineIdHelper::getMachineId();
    }
    return m_machineId;
}

QString LoginDialog::getServerUrl() const
{
    return m_serverUrlEdit->text().trimmed();
}
