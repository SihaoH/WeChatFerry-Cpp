#pragma once
#include "DataUtil.h"
#include <QMutex>
#include <QCoreApplication>

struct AppMessage_t
{
    QStringList conent;
    qint64 timestamp;
};

class Application : public QCoreApplication
{
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application();

    void startReceiveMessage();
    void stopReceiveMessage();
    void waitForLogin();
    void pullContacts();

private:
    QByteArray sendRequestRaw(const Request& req);
    QSharedPointer<Response> sendRequest(const Request& req);
    void reloadConfig(bool is_first = false);
    void initWCF();
    void initNngClient();
    void initHandler();
    void initConfigWatcher();
    bool isWeChatRunning();
    void asyncReceiving();

private slots:
    void onHandle();

private:
    int nngPort = 16888;
    int isReceiving = false;
    int waitTime = 0;
    class QTimer* handleTimer = nullptr;
    class NngSocket* reqSocket = nullptr;
    class NngSocket* msgSocket = nullptr;
    class ChatRobot* chatRobot = nullptr;
    class QFileSystemWatcher * configWatcher = nullptr;
    QList<QString> whiteList;
    QMap<QString, AppMessage_t> msgList;
    QMutex mutex;
    const QString configFile = "./config.json";
};
