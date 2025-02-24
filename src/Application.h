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
    bool autoDelImg = false;
    class Client* client = nullptr;
    class QTimer* handleTimer = nullptr;
    class ChatRobot* chatRobot = nullptr;
    class QFileSystemWatcher * configWatcher = nullptr;
    QList<QString> whiteList;
    QMap<QString, MessageSection> msgMap;
    QMutex mutex;
};
