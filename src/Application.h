#pragma once
#include "DataUtil.h"
#include <QMutex>
#include <QCoreApplication>

struct AppMessage_t
{
    QStringList conent;
    qint64 timestamp;
};
struct AppContact_t
{
    QString name;
    QString attr;
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
    void initWCF();
    void initNngClient();
    void initChatRobot();
    bool isWeChatRunning();
    void asyncReceiving();

private slots:
    void onHandle();

private:
    int nngPort = 16888;
    int isReceiving = false;
    class QTimer* handleTimer = nullptr;
    class NngSocket* reqSocket = nullptr;
    class NngSocket* msgSocket = nullptr;
    class ChatRobot* chatRobot = nullptr;
    QMap<QString, AppMessage_t> msgList;
    QMap<QString, AppContact_t> contactList;
    QMutex mutex;
};
