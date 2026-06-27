#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCheckBox>

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    
    QString getUsername() const;
    QString getMachineId() const;
    QString getServerUrl() const;
    bool isRegistered() const { return m_isRegistered; }

private slots:
    void onRegister();
    void onLogin();
    void onRegisterModeChanged(bool checked);

private:
    void setupUI();
    bool validateInput(bool isRegister);
    
    QString getMachineIdInternal() const;
    
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_serverUrlEdit;
    QCheckBox* m_registerModeCheck;
    QPushButton* m_registerBtn;
    QPushButton* m_loginBtn;
    QPushButton* m_cancelBtn;
    
    QString m_machineId;  // 缓存的机器码
    bool m_isRegistered;
};

#endif // LOGINDIALOG_H
