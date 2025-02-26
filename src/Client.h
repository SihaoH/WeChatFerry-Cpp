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
        Refer = 0x31, // 引用
        Other = 0xFFFFFFFF // 其他未实现处理的类型
    };
};

class Client : public QObject
{
    Q_OBJECT
public:
    struct Message
    {
        MsgType::Type type = MsgType::Other;
        QString sender; // 发送者的wxid
        QString name; // 发送者的名称，优先级：群昵称>备注>微信名
        QString roomid; // 群聊id，如果是私聊则为空
        QString content; // 内容，如果是图片或视频等文件，则为文件路径
    };
    struct Contact
    {
        QString wxid;
        QString name;
        QString attr; // 群聊、男、女、个人
    };
    struct Options
    {
        QList<MsgType::Type> types;
        bool needGroup = true;
        bool onlyAter = true;
    };
    using SqlResult = QList<QMap<QString, QVariant>>;

public:
    Client(QObject* parent, int port);
    ~Client();

    void sendText(const QString& wxid, const QString& txt); // 纯文本，可以@他人，例子："哈哈哈，@{wxid_xx}来，来财"
    void sendRichText(const QMap<QString, QString>& args); // 参数比较多，直接用map
    void sendXml(const QString& wxid, const QString& xml, int type, const QString& img = QString());
    void sendImage(const QString& wxid, const QString& img);
    void sendEmotion(const QString& wxid, const QString& gif);
    void sendFile(const QString& wxid, const QString& file);
    void sendPatPat(const QString& roomid, const QString& wxid);

    void inviteRoomMembers(const QString& roomid, const QString& wxid);
    void removeRoomMembers(const QString& roomid, const QString& wxid);

    bool isLogin();
    Contact getSelfInfo();
    QList<Contact> getFriendList();
    QString nameInGroup(const QString& roomid, const QString& wxid);

    void setReceiveMessage(bool enabled);
    // opt为关心消息的选项，若不关心则会过滤掉，等待新的消息
    Message receiveMessage(Options opt = Options());

private:
    QByteArray sendRequestRaw(const Request& req);
    QSharedPointer<Response> sendRequest(const Request& req);
    void sendFileRequest(Functions func, const QString& wxid, const QString& file);
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
