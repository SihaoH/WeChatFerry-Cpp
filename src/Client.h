#pragma once
#include "DataUtil.h"
#include <QObject>

class MsgType
{
public:
    enum Type
    {
        Pyq = 0x00,
        Text = 0x01,
        Image = 0x03,
        Audio = 0x22,
        Video = 0x2B,
        Other = 0xFFFFFFFF //暂未实现处理的类型
    };
};

class Client : public QObject
{
    Q_OBJECT
public:
    struct Message
    {
        MsgType::Type type = MsgType::Other;
        QString sender;
        QString content;
    };
    struct Contact
    {
        QString wxid;
        QString name;
        QString attr; // 群聊/男/女/个人
    };

public:
    Client(QObject* parent, int port);
    ~Client();

    void sendText(const QString& wxid, const QString& txt);
    void sendXml(const QString& wxid, const QString& xml);
    void sendImage(const QString& wxid, const QString& img);
    void sendEmotion(const QString& wxid, const QString& gif);
    void sendFile(const QString& wxid, const QString& file);

    bool isLogin();
    Contact getSelfInfo();
    QList<Contact> getContacts();

    void setReceiveMessage(bool enabled);
    Message receiveMessage();

private:
    QByteArray sendRequestRaw(const Request& req);
    QSharedPointer<Response> sendRequest(const Request& req);

private:
    const int dlTimes = 10;
    int nngPort;
    class NngSocket* reqSocket = nullptr;
    class NngSocket* msgSocket = nullptr;
};
