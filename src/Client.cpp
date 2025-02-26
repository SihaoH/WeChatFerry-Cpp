#include "Client.h"
#include "Logger.h"
#include "NngSocket.h"
#include "DataUtil.h"
#include <QThread>
#include <QRegularExpression>
#include <QDir>

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

void Client::sendFileRequest(Functions func, const QString& wxid, const QString& file)
{
    Request req = Request_init_default;
    req.func = func;
    req.which_msg = Request_file_tag;
    QByteArray receiver = wxid.toUtf8();
    QByteArray path = file.toUtf8();
    req.msg.file.receiver = receiver.data();
    req.msg.file.path = path.data();
    sendRequest(req);
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
            case 1: var = c.toInt(); break;
            case 2: var = c.toFloat(); break;
            case 3: var = QString(c); break;
            case 4: var = c; break;
            default: break;
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

    // 只有主动改了群昵称的才会在RoomData里面显示，不然要去Contacat总表里面获取
    auto crs = querySQL("MicroMsg.db", QString("SELECT RoomData FROM ChatRoom WHERE ChatRoomName = '%1';").arg(wxid));
    if (crs.isEmpty()) {
        LOG(err) << "没有获取到该群聊的信息：" << wxid;
        return;
    }
    auto bs = crs[0]["RoomData"].toByteArray();
    auto& group_info = GroupMemberMap[wxid];
    // 没有找到解码RoomData的方法，只能根据数据格式来强行解码
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
        QString wxid = QByteArray(bs.data() + i + 4, id_size);
        if (name_size > 0) {
            group_info[wxid] = QByteArray(bs.data() + name_offset + 3, name_size);
        } else {
            group_info[wxid] = contactMap[wxid];
        }
        i += block_size + 2;
    }
}

void Client::sendText(const QString& wxid, const QString& txt)
{
    QString after_txt = txt;
    QString aters_str;
    if (wxid.endsWith("@chatroom")) {
        QRegularExpression regex("@\\{([^}]+)\\}");
        QRegularExpressionMatchIterator iter = regex.globalMatch(txt);
        QStringList ater_list;
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            QString captured = match.captured(1);
            after_txt.replace(QString("{%1}").arg(captured), nameInGroup(wxid, captured) + " ");

            ater_list.append(captured);
        }
        aters_str = ater_list.join(',');
    }
    Request req = Request_init_default;
    req.func = Functions_FUNC_SEND_TXT;
    req.which_msg = Request_txt_tag;
    QByteArray receiver = wxid.toUtf8();
    QByteArray msg = after_txt.toUtf8();
    QByteArray aters = aters_str.toUtf8();
    req.msg.txt.receiver = receiver.data();
    req.msg.txt.msg = msg.data();
    req.msg.txt.aters = aters.data();
    sendRequest(req);
}

void Client::sendRichText(const QMap<QString, QString>& args)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_SEND_RICH_TXT;
    req.which_msg = Request_rt_tag;
    QByteArray name = args["name"].toUtf8(); // 左下显示的名字
    QByteArray account = args["account"].toUtf8(); // 填公众号id可以显示对应的头像
    QByteArray title = args["title"].toUtf8(); // 标题，最多两行
    QByteArray digest = args["digest"].toUtf8(); // 摘要，三行
    QByteArray url = args["url"].toUtf8(); // 点击后跳转的链接
    QByteArray thumburl = args["thumburl"].toUtf8(); // 缩略图的链接
    QByteArray receiver = args["receiver"].toUtf8(); // 接收人, wxid 或者 roomid
    req.msg.rt.name = name.data();
    req.msg.rt.account = account.data();
    req.msg.rt.title = title.data();
    req.msg.rt.digest = digest.data();
    req.msg.rt.url = url.data();
    req.msg.rt.thumburl = thumburl.data();
    req.msg.rt.receiver = receiver.data();
    sendRequest(req);
}

void Client::sendXml(const QString& wxid, const QString& xml, int type, const QString& img)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_SEND_XML;
    req.which_msg = Request_xml_tag;
    QByteArray receiver = wxid.toUtf8();
    QByteArray msg = xml.toUtf8();
    QByteArray _img = img.toUtf8();
    req.msg.xml.receiver = receiver.data();
    req.msg.xml.content = msg.data();
    req.msg.xml.type = type;
    req.msg.xml.path = _img.data();
    sendRequest(req);
}

void Client::sendImage(const QString& wxid, const QString& img)
{
    sendFileRequest(Functions_FUNC_SEND_IMG, wxid, img);
}

void Client::sendEmotion(const QString& wxid, const QString& gif)
{
    sendFileRequest(Functions_FUNC_SEND_EMOTION, wxid, gif);
}

void Client::sendFile(const QString& wxid, const QString& file)
{
    sendFileRequest(Functions_FUNC_SEND_FILE, wxid, file);
}

void Client::sendPatPat(const QString& roomid, const QString& wxid)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_SEND_PAT_MSG;
    req.which_msg = Request_pm_tag;
    QByteArray _roomid = roomid.toUtf8();
    QByteArray _wxid = wxid.toUtf8();
    req.msg.pm.roomid = _roomid.data();
    req.msg.pm.wxid = _wxid.data();
    sendRequest(req);
}

void Client::inviteRoomMembers(const QString& roomid, const QString& wxid)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_INV_ROOM_MEMBERS;
    req.which_msg = Request_m_tag;
    QByteArray _roomid = roomid.toUtf8();
    QByteArray _wxid = wxid.toUtf8();
    req.msg.m.roomid = _roomid.data();
    req.msg.m.wxids = _wxid.data();
    sendRequest(req);
}

void Client::removeRoomMembers(const QString& roomid, const QString& wxid)
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_DEL_ROOM_MEMBERS;
    req.which_msg = Request_m_tag;
    QByteArray _roomid = roomid.toUtf8();
    QByteArray _wxid = wxid.toUtf8();
    req.msg.m.roomid = _roomid.data();
    req.msg.m.wxids = _wxid.data();
    sendRequest(req);
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

QString Client::nameInGroup(const QString& roomid, const QString& wxid)
{
    if (GroupMemberMap.contains(roomid)) {
        if (!GroupMemberMap[roomid].contains(wxid)) {
            // 已有的群成员里没有此人信息，可能是新加入的，需要更新群信息
            pullGroupMembers(roomid, true);
        }
    } else {
        pullGroupMembers(roomid);
    }
    return GroupMemberMap[roomid][wxid];
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
    bool has_useful = false;
    while (has_useful == false) {
        auto rsp = DataUtil::toResponse(msgSocket->waitForRecv());
        auto wxmsg = rsp->msg.wxmsg;

        msg = Message();
        msg.type = (MsgType::Type)wxmsg.type;
        if (!opt.types.contains(msg.type)) {
            continue;
        }

        if (wxmsg.is_group) {
            if (opt.needGroup) {
                msg.roomid = wxmsg.roomid;
                msg.sender = wxmsg.sender;
                msg.name = nameInGroup(wxmsg.roomid, wxmsg.sender);
            } else {
                continue;
            }
        } else {
            msg.sender = QString(wxmsg.sender);
            msg.name = friendMap[msg.sender].name;
        }

        switch (msg.type) {
        case MsgType::Text: {
            msg.content.append(wxmsg.content);
            if (opt.onlyAter && QString(wxmsg.xml).contains("<atuserlist>")) {
                has_useful = QString(wxmsg.xml).contains(selfInfo.wxid);
            }
            break;
        }
        case MsgType::Refer: {
            QString content = wxmsg.content;
            // 只能提取消息的文本，被引用的内容可能是各种类型
            content = content.section(QRegularExpression("<title>|</title>", QRegularExpression::DotMatchesEverythingOption), 1, 1);
            msg.type = MsgType::Text; // 先当做纯文本处理
            msg.content.append(content);
            if (opt.onlyAter && QString(wxmsg.xml).contains("<atuserlist>")) {
                has_useful = QString(wxmsg.xml).contains(selfInfo.wxid);
            }
            break;
        }
        case MsgType::Image: {
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
            req.msg.dec.dst = (char*)".//images/"; // 这里要双斜杠，不然spy创建的文件夹名称会缺少首字母i
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
            has_useful = !img_file.isEmpty();
            break;
        }
        case MsgType::Audio: {
            Request req = Request_init_default;
            req.func = Functions_FUNC_GET_AUDIO_MSG;
            req.which_msg = Request_am_tag;
            req.msg.am.id = wxmsg.id;
            req.msg.am.dir = (char*)"./audio/";
            QDir dir("./audio/");
            if (!dir.exists()) {
                dir.mkpath(".");
            }
            int times = 0;
            QString audio_file;
            while (audio_file.isEmpty()) {
                if (times += 1 > dlTimes) {
                    LOG(err) << "获取语音数据失败！";
                    break;
                }
                auto rsp = sendRequest(req);
                audio_file = QString(rsp->msg.str);
                QThread::sleep(1);
            }
            msg.content.append(audio_file);
            has_useful = !audio_file.isEmpty();
            break;
        }
        case MsgType::Video: {
            Request req = Request_init_default;
            req.func = Functions_FUNC_DOWNLOAD_ATTACH;
            req.which_msg = Request_att_tag;
            req.msg.att.id = wxmsg.id;
            req.msg.att.thumb = wxmsg.thumb;
            if (sendRequest(req)->msg.status == 0) {
                QString video_file = wxmsg.thumb;
                video_file.replace(".jpg", ".mp4");
                msg.content.append(video_file);
                has_useful = true;
            } else {
                LOG(err) << "下载视频失败！";
            }
            break;
        }
        default:
            break;
        }
    }

    return msg;
}

