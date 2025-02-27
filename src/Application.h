#pragma once
#include "Client.h"
#include <QMutex>
#include <QCoreApplication>

class Application : public QCoreApplication
{
    Q_OBJECT

public:
    struct MessageSection {
        QList<Client::Message> list;
        qint64 timestamp;
    };

public:
    Application(int& argc, char** argv);
    ~Application();

    void startReceiveMessage();
    void stopReceiveMessage();
    void waitForLogin();
    void dumpFriendList();

private:
    void reloadConfig(bool is_first = false);
    void initWCF();
    void initClient();
    void initHandler();
    void initConfigWatcher();
    bool isWeChatRunning();
    void asyncReceiving();

private slots:
    void onHandle();

private:
    const QString configFile = "./config.json";
    int nngPort = 16888;
    int waitTime = 0;
    bool isReceiving = false;
    bool isAutoReplying = false;
    class Client* client = nullptr;
    class QTimer* handleTimer = nullptr;
    class ChatRobot* chatRobot = nullptr;
    class QFileSystemWatcher * configWatcher = nullptr;
    QMap<QString, MessageSection> msgMap;
    QMutex mutex;
    
    // 聊天相关配置
    struct ChatConfig {
        QList<QString> autoReply;  // 自动回复的群列表
        bool onlyAter = false;  // 是否只在被@时回复
    } chatConfig;
    
    // 邀请相关配置
    struct InviteConfig {
        QString keyword;
        QString reply;
        QString roomId;
    } inviteConfig;

    // 管理员相关配置
    struct AdminConfig {
        QString wxid;
        QString startCmd;
        QString stopCmd;
        QString quitCmd;
    } adminConfig;
};
