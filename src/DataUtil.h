#pragma once
#include "wcf.pb.h"
#include <QByteArray>
#include <QSharedPointer>

class DataUtil
{
public:
    static QByteArray encode(const Request& req);
    static QByteArray encode(const Response& req);
    static QSharedPointer<Request> toRequest(const QByteArray& data);
    static QSharedPointer<Response> toResponse(const QByteArray& data);
};
