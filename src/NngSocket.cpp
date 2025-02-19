#include "NngSocket.h"
#include "Logger.h"
#include <nng/protocol/pair1/pair.h>

#define CHECK(CALL) checkCall(CALL, __LINE__)

NngSocket::NngSocket()
{
}

NngSocket::~NngSocket()
{
    disconnectFromHost();
}

void NngSocket::connectToHost(const QString& host, quint16 port)
{
    if (socket.id == 0) {
        QString url = QString("%1:%2").arg(host).arg(port);
        CHECK(nng_pair1_open(&socket));
        CHECK(nng_dial(socket, url.toLocal8Bit(), NULL, 0));
    } else {
        LOG(warn) << QStringLiteral("NngSocket已连接，请先断开后再发起连接");
    }
}

void NngSocket::disconnectFromHost()
{
    if (socket.id != 0) {
        CHECK(nng_socket_close(socket));
        socket = NNG_SOCKET_INITIALIZER;
    } else {
        LOG(warn) << QStringLiteral("NngSocket未连接！");
    }
}

void NngSocket::setSendTimeout(int msec)
{
    if (socket.id != 0) {
        CHECK(nng_setopt_ms(socket, NNG_OPT_SENDTIMEO, msec));
    } else {
        LOG(warn) << QStringLiteral("NngSocket未连接！");
    }
}

void NngSocket::setRecvTimeout(int msec)
{
    if (socket.id != 0) {
        CHECK(nng_setopt_ms(socket, NNG_OPT_RECVTIMEO, msec));
    } else {
        LOG(warn) << QStringLiteral("NngSocket未连接！");
    }
}

bool NngSocket::send(const QByteArray& data)
{
    if (data.isEmpty()) {
        LOG(warn) << QStringLiteral("NngSocket发送的数据为空！");
        return false;
    }
    if (socket.id != 0) {
        return CHECK(nng_send(socket, (void*)data.data(), data.size(), 0));
    } else {
        LOG(warn) << QStringLiteral("NngSocket未连接！");
        return false;
    }
}

QByteArray NngSocket::waitForRecv()
{
    if (socket.id != 0) {
        uint8_t* out = nullptr;
        size_t size = 0;
        CHECK(nng_recv(socket, &out, &size, NNG_FLAG_ALLOC));
        QByteArray data((const char*)out, size);
        nng_free(out, size);
        return data;
    } else {
        LOG(warn) << QStringLiteral("NngSocket未连接！");
        return QByteArray();
    }
}

bool NngSocket::checkCall(int rv, int line)
{
    if (rv != 0) {
        LOG(err) << QStringLiteral("nng调用失败，行号%1, %2").arg(line).arg(nng_strerror(rv));
    }
    return rv == 0;
}
