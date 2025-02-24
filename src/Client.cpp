#include "Client.h"
#include "Logger.h"
#include "NngSocket.h"
#include "DataUtil.h"
#include <QThread>

constexpr const char* NNG_HOST = "tcp://127.0.0.1";

Client::Client(QObject* parent, int port)
    : QObject(parent)
    , nngPort(port)
{
    LOG(info) << "初始化客户端：连接NNG服务端...";
    reqSocket = new NngSocket();
    msgSocket = new NngSocket();
    reqSocket->connectToHost(NNG_HOST, nngPort);
    reqSocket->setSendTimeout(5000);
    reqSocket->setRecvTimeout(5000);

    pullSelfInfo();
    pullContacts();
}

Client::~Client()
{
    setReceiveMessage(false);
    reqSocket->disconnectFromHost();
    delete reqSocket;
    delete msgSocket;
}

QByteArray Client::sendRequestRaw(const Request& req)
{
    reqSocket->send(DataUtil::encode(req));
    return reqSocket->waitForRecv();
}

QSharedPointer<Response> Client::sendRequest(const Request& req)
{
    return DataUtil::toResponse(sendRequestRaw(req));
}

Client::SqlResult Client::querySQL(const QString& db, const QString& sql)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_EXEC_DB_QUERY;
    req.which_msg = Request_query_tag;
    auto db_str = db.toUtf8();
    auto sql_str = sql.toUtf8();
    req.msg.query.db = db_str.data();
    req.msg.query.sql = sql_str.data();
    auto rows = DataUtil::getDatabaseRows(sendRequestRaw(req));
    SqlResult result;
    for (const auto& row : rows) {
        QMap<QString, QVariant> ret_row;
        for (const auto& field : row) {
            auto c = QByteArray((char*)field.content.data(), field.content.size());
            QVariant var;
            switch (field.type)
            {
            case 1:
                var = c.toInt();
                break;
            case 2:
                var = c.toFloat();
                break;
            case 3:
                var = QString(c);
                break;
            case 4:
                var = c;
            default:
                break;
            }
            ret_row[QString::fromStdString(field.column)] = var;
        }
        result.append(ret_row);
    }
    return result;
}

void Client::pullSelfInfo()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_GET_USER_INFO;
    req.which_msg = Request_func_tag;
    auto rsp = sendRequest(req);
    selfInfo.wxid = rsp->msg.ui.wxid;
    selfInfo.name = rsp->msg.ui.name;
}

void Client::pullContacts()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_GET_CONTACTS;
    req.which_msg = Request_func_tag;
    auto rsp = sendRequestRaw(req);
    auto rsp_contacts = DataUtil::getContacts(rsp);
    friendMap.clear();
    for (const auto& rsp_contact : rsp_contacts) {
        Contact contact;
        contact.wxid = QString::fromStdString(rsp_contact.wxid);
        QStringList not_friends = {
            "fmessage", // 朋友推荐消息
            "medianote", // 语音记事本
            "floatbottle", // 漂流瓶
            "filehelper", // 文件传输助手
            "newsapp" // 新闻
        };
        if (contact.wxid.isEmpty() || 
            contact.wxid.startsWith("gh_") || // 公众号
            not_friends.contains(contact.wxid)) {
            // 特殊账号不处理
            continue;
        }

        contact.name = QString::fromStdString(rsp_contact.remark.empty() ? rsp_contact.name : rsp_contact.remark);
        if (contact.name.isEmpty()) contact.name = "<null>";
        if (contact.wxid.endsWith("@chatroom")) {
            contact.attr = "群聊";
        } else {
            if (rsp_contact.gender == 1) {
                contact.attr = "男";
            } else if(rsp_contact.gender == 2) {
                contact.attr = "女";
            } else {
                contact.attr = "个人";
            }
        }
        friendMap.insert(contact.wxid, contact);
    }
}

void Client::pullGroupMembers(const QString& wxid, bool refresh)
{
    if (contactMap.isEmpty() || refresh) {
        auto all_list = querySQL("MicroMsg.db", "SELECT UserName, NickName FROM Contact;");
        for (const auto& contact : all_list) {
            contactMap[contact["UserName"].toString()] = contact["NickName"].toString();
        }
    }

    auto crs = querySQL("MicroMsg.db", QString("SELECT RoomData FROM ChatRoom WHERE ChatRoomName = '%1';").arg(wxid));
    if (crs.isEmpty()) {
        LOG(err) << "没有获取到该群聊的信息：" << wxid;
        return;
    }
    auto bs = crs[0]["RoomData"].toByteArray();
    auto& group_info = GroupMemberMap[wxid];
    // 没有找到解码RoomData的方法，只能硬解
    for (int i = 0; i < bs.size(); i) {
        int block_size = 0;
        int id_size = 0;
        int name_size = 0;
        if (bs[i] == 10) {
            block_size = bs[i + 1];
        } else {
            break;
        }
        if (bs[i + 2] == 10) {
            id_size = bs[i + 3];
        }
        int name_offset = i + 3 + id_size;
        if (bs[name_offset + 1] == 18) {
            name_size = bs[name_offset + 2];
        }
        QString wxid = QByteArray(bs.constData() + i + 4, id_size);
        if (name_size > 0) {
            group_info[wxid] = QByteArray(bs.constData() + name_offset + 3, name_size);
        } else {
            group_info[wxid] = contactMap[wxid];
        }
        i += block_size + 2;
    }
}

void Client::sendText(const QString& wxid, const QString& txt)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_SEND_TXT;
    req.which_msg = Request_txt_tag;
    QByteArray receiver = wxid.toUtf8();
    QByteArray msg = txt.toUtf8();
    req.msg.txt.receiver = (char*)receiver.constData();
    req.msg.txt.msg = (char*)msg.constData();
    sendRequest(req);
}

void Client::sendXml(const QString& wxid, const QString& xml)
{
    // TODO
}

void Client::sendImage(const QString& wxid, const QString& img)
{
    // TODO
}

void Client::sendEmotion(const QString& wxid, const QString& gif)
{
    // TODO
}

void Client::sendFile(const QString& wxid, const QString& file)
{
    // TODO
}

bool Client::isLogin()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_IS_LOGIN;
    req.which_msg = Response_status_tag;
    auto rsp = sendRequest(req);
    return rsp->msg.status > 0;
}

Client::Contact Client::getSelfInfo()
{
    return selfInfo;
}

QList<Client::Contact> Client::getFriendList()
{
    return friendMap.values();
}

void Client::setReceiveMessage(bool enabled)
{
    Request req = Request_init_default;
    req.which_msg = Request_func_tag;
    if (enabled) {
        req.func = Functions_FUNC_ENABLE_RECV_TXT;
        auto rsp = sendRequest(req);
        if (rsp->msg.status == 0) {
            msgSocket->connectToHost(NNG_HOST, nngPort + 1);
        }
    } else {
        msgSocket->disconnectFromHost();
        req.func = Functions_FUNC_DISABLE_RECV_TXT;
        sendRequest(req);
    }
}

Client::Message Client::receiveMessage(Client::Options opt)
{
    // 获取wx接收的信息
    Message msg;
    auto rsp = DataUtil::toResponse(msgSocket->waitForRecv());
    auto wxmsg = rsp->msg.wxmsg;
    if (wxmsg.is_group) {
        if (GroupMemberMap.contains(wxmsg.roomid)) {
            if (!GroupMemberMap[wxmsg.roomid].contains(wxmsg.sender)) {
                // 已有的群成员里没有此人信息，可能是新加入的，需要更新群信息
                pullGroupMembers(wxmsg.roomid, true);
            }
        } else {
            pullGroupMembers(wxmsg.roomid);
        }
        msg.sender = QString(wxmsg.roomid);
        msg.content.append(GroupMemberMap[wxmsg.roomid][wxmsg.sender] + ": ");
    } else {
        msg.sender = QString(wxmsg.sender);
    }

    switch (wxmsg.type) {
    case MsgType::Text: {
        msg.type = MsgType::Text;
        msg.content.append(wxmsg.content);
        break;
    }
    case MsgType::Image: {
        msg.type = MsgType::Image;
        Request req = Request_init_default;
        req.func = Functions_FUNC_DOWNLOAD_ATTACH;
        req.which_msg = Request_att_tag;
        req.msg.att.id = wxmsg.id;
        req.msg.att.extra = wxmsg.extra;
        if (sendRequest(req)->msg.status != 0) {
            LOG(err) << "下载图片失败！";
        }
        req = Request_init_default;
        req.func = Functions_FUNC_DECRYPT_IMAGE;
        req.which_msg = Request_dec_tag;
        req.msg.dec.src = wxmsg.extra;
        req.msg.dec.dst = (char*)".//images/"; // 这里要双斜杠，不然创建的文件夹名称会缺少首字母i
        int times = 0;
        QString img_file;
        while (img_file.isEmpty()) {
            if (times += 1 > dlTimes) {
                LOG(err) << "解密图片失败！";
                break;
            }
            auto rsp = sendRequest(req);
            img_file = QString(rsp->msg.str);
            QThread::sleep(1);
        }
        msg.content.append(img_file);
        break;
    }
    case MsgType::Video: {
        msg.type = MsgType::Video;
        Request req = Request_init_default;
        req.func = Functions_FUNC_DOWNLOAD_ATTACH;
        req.which_msg = Request_att_tag;
        req.msg.att.id = wxmsg.id;
        req.msg.att.thumb = wxmsg.thumb;
        if (sendRequest(req)->msg.status == 0) {
            QString video_file = wxmsg.thumb;
            video_file.replace(".jpg", ".mp4");
            msg.content.append(video_file);
        } else {
            LOG(err) << "下载视频失败！";
        }
        break;
    }
    default:
        break;
    }
    return msg;
}

