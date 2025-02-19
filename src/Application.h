#pragma once
#include <nng/nng.h>

#include <QCoreApplication>

class Application : public QCoreApplication
{
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application();

    void startReceiveMessage();
    void stopReceiveMessage();

private:
    void initWCF();
    void initNngClient();
    bool isWeChatRunning();

private slots:
    void onHandle();

private:
    int nngPort = 16888;
    int isReceiving = false;
    class QTimer* handleTimer = nullptr;
    class NngSocket* reqSocket = nullptr;
    class NngSocket* msgSocket = nullptr;
};
