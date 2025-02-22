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
    reqSocket->setRecvTimeout(500);
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
    Request req = Request_init_default;
    req.func = Functions_FUNC_GET_USER_INFO;
    req.which_msg = Response_status_tag;
    auto rsp = sendRequest(req);
    Contact self;
    self.wxid = rsp->msg.ui.wxid;
    self.name = rsp->msg.ui.name;
    return self;
}

QList<Client::Contact> Client::getContacts()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_GET_CONTACTS;
    req.which_msg = Request_func_tag;
    auto rsp = sendRequestRaw(req);
    auto rsp_contacts = DataUtil::getContacts(rsp);
    QList<Contact> list;
    for (const auto& rsp_contact : rsp_contacts) {
        Contact contact;
        contact.wxid = QString::fromStdString(rsp_contact.wxid);
        if (contact.wxid.isEmpty() || contact.wxid.startsWith("gh_")) {
            // 公众号不处理
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
        list.append(contact);
    }
    return list;
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

Client::Message Client::receiveMessage()
{
    // 获取wx接收的信息
    Message msg;
    auto rsp = DataUtil::toResponse(msgSocket->waitForRecv());
    auto wxmsg = rsp->msg.wxmsg;
    if (wxmsg.is_group) {
        msg.sender = QString(wxmsg.roomid);
    } else {
        msg.sender = QString(wxmsg.sender);
    }

    switch (wxmsg.type) {
    case MsgType::Text: {
        msg.type = MsgType::Text;
        msg.content = QString(wxmsg.content);
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
        while (msg.content.isEmpty()) {
            if (times += 1 > dlTimes) {
                LOG(err) << "解密图片失败！";
                break;
            }
            auto rsp = sendRequest(req);
            msg.content = QString(rsp->msg.str);
            QThread::sleep(1);
        }
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
            msg.content = (wxmsg.thumb);
            msg.content.replace(".jpg", ".mp4");
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

