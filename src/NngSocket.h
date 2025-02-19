#pragma once
#include <nng/nng.h>
#include <QString>

class NngSocket
{
public:
    NngSocket();
    ~NngSocket();

    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();

    void setSendTimeout(int msec);
    void setRecvTimeout(int msec);

    bool send(const QByteArray& data);
    QByteArray waitForRecv();

private:
    bool checkCall(int rv, int line);

private:
    nng_socket socket = NNG_SOCKET_INITIALIZER;
};
