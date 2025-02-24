#pragma once
#include "DataUtil.h"
#include <QObject>
#include <QMap>
#include <QVariant>

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
        Quote = 0x31000031, //引用
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
    struct Options
    {
        bool needGroup = true;
        bool onlyAter = true;
        bool needImage = true;
        bool downloadVideo = true;
    };
    using SqlResult = QList<QMap<QString, QVariant>>;

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
    QList<Contact> getFriendList();

    void setReceiveMessage(bool enabled);
    Message receiveMessage(Options opt = Options());

private:
    QByteArray sendRequestRaw(const Request& req);
    QSharedPointer<Response> sendRequest(const Request& req);
    SqlResult querySQL(const QString& db, const QString& sql);

    void pullSelfInfo();
    void pullContacts();
    void pullGroupMembers(const QString& wxid, bool refresh = false);

private:
    const int dlTimes = 10;
    int nngPort;
    class NngSocket* reqSocket = nullptr;
    class NngSocket* msgSocket = nullptr;

    Contact selfInfo;
    QMap<QString, Contact> friendMap;
    QMap<QString, QString> contactMap;
    QMap<QString, QMap<QString, QString>> GroupMemberMap;
};
