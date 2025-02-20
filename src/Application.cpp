#include "Application.h"
#include "Logger.h"
#include "sdk.h"
#include "NngSocket.h"
#include "ChatRoBot.h"
#include <QTimer>
#include <QProcess>
#include <QtConcurrent>

constexpr const char* NNG_HOST = "tcp://127.0.0.1";

Application::Application(int& argc, char** argv)
    : QCoreApplication(argc, argv)
{
    Logger::instance()->init("app");

    if (argc >= 2) {
        nngPort = QString(argv[1]).toInt();
    }
    initWCF();
    initNngClient();

    initChatRobot();
    waitForLogin();
    pullContacts();
    startReceiveMessage();

    handleTimer = new QTimer(this);
    handleTimer->setInterval(1000);
    handleTimer->start();
    connect(handleTimer, &QTimer::timeout, this, &Application::onHandle);
}

Application::~Application()
{
    stopReceiveMessage();
    delete chatRobot;
    delete reqSocket;
    delete msgSocket;
}

bool Application::isWeChatRunning()
{
    QProcess process;
    process.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq WeChat.exe");
    process.waitForFinished();

    // 读取命令输出
    QByteArray result = process.readAllStandardOutput();
    QString output = QString::fromLocal8Bit(result);

    // 检查输出中是否包含进程名
    return output.contains("WeChat.exe", Qt::CaseInsensitive);
}

QByteArray Application::sendRequestRaw(const Request& req)
{
    reqSocket->send(DataUtil::encode(req));
    return reqSocket->waitForRecv();
}

QSharedPointer<Response> Application::sendRequest(const Request& req)
{
    return DataUtil::toResponse(sendRequestRaw(req));
}

void Application::initWCF()
{
    if (isWeChatRunning()) {
        LOG(info) << QStringLiteral("微信正在运行，跳过注入wcf步骤！");

    } else {
        LOG(info) << QStringLiteral("微信没有运行，启动微信并注入wcf");
        int ret = 0;
    #if _DEBUG
        ret = WxInitSDK(true, nngPort);
    #else
        ret = WxInitSDK(false, nngPort);
    #endif
        if (ret != 0) {
            LOG(info) << QStringLiteral("注入wcf失败，请确认并重启程序！");
        }
    }
}

void Application::initNngClient()
{
    reqSocket = new NngSocket();
    msgSocket = new NngSocket();
    reqSocket->connectToHost(NNG_HOST, nngPort);
    reqSocket->setSendTimeout(5000);
    reqSocket->setRecvTimeout(500);
}

void Application::initChatRobot()
{
    chatRobot = new ChatRobot;
    chatRobot->setPrompt(QStringLiteral("你扮演一个聊天机器人，会有不同的人来跟你说话，你需要直接做出回复，每条回复尽量简短，不超过50个字"));
}

void Application::waitForLogin()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_IS_LOGIN;
    req.which_msg = Response_status_tag;
    for (;;) {
        QThread::sleep(1);
        auto rsp = sendRequest(req);
        if ((*rsp).msg.status > 0) {
            break;
        }
    }
}

void Application::pullContacts()
{
    Request req = Request_init_default;
    req.func = Functions_FUNC_GET_CONTACTS;
    req.which_msg = Request_func_tag;
    auto rsp = sendRequestRaw(req);
    auto contacts = DataUtil::getContacts(rsp);
    contactList.clear();
    for (const auto& contact : contacts) {
        auto wxid = QString::fromStdString(contact.wxid);
        if (wxid.isEmpty() || wxid.startsWith("gh_")) {
            // 公众号不处理
            continue;
        }

        AppContact_t app_contact;
        app_contact.name = QString::fromStdString(contact.remark.empty() ? contact.name : contact.remark);
        if (wxid.endsWith("@chatroom")) {
            app_contact.attr = QStringLiteral("群聊");
        } else {
            if (contact.gender == 1) {
                app_contact.attr = QStringLiteral("男");
            } else if(contact.gender == 2) {
                app_contact.attr = QStringLiteral("女");
            } else {
                LOG(debug) << QStringLiteral("未知属性的联系人：") << app_contact.name;
            }
        }
        contactList.insert(wxid, app_contact);
    }
}

void Application::startReceiveMessage()
{
    if (isReceiving) return;

    LOG(info) << QStringLiteral("开始接收微信消息...");
    Request req = Request_init_default;
    req.func = Functions_FUNC_ENABLE_RECV_TXT;
    req.which_msg = Request_func_tag;
    sendRequest(req);
    msgSocket->connectToHost(NNG_HOST, nngPort + 1);

    isReceiving = true;
    QtConcurrent::run([this]() {
        asyncReceiving();
    });
}

void Application::stopReceiveMessage()
{
    LOG(info) << QStringLiteral("停止接收微信消息");
    isReceiving = false;

    Request req = Request_init_default;
    req.func = Functions_FUNC_DISABLE_RECV_TXT;
    req.which_msg = Request_func_tag;
    sendRequest(req);
}

void Application::asyncReceiving()
{
    while (isReceiving) {
        // 获取wx接收的信息
        auto rsp = DataUtil::toResponse(msgSocket->waitForRecv());
        auto wxmsg = (*rsp).msg.wxmsg;
        if (QString(wxmsg.content).startsWith("<?xml version="1.0"?>")) {
            // 特殊消息，先不处理
            continue;
        }

        QMutexLocker locker(&mutex);
        QString wxid;
        if (wxmsg.is_group) {
            wxid = wxmsg.roomid;
            auto& app_msg = msgList[wxid];
            QString sentence = QStringLiteral("有人说: ");
            if (contactList.contains(wxmsg.sender)) {
                sentence = contactList[wxmsg.sender].name + QStringLiteral("说: ");
            }
            sentence.append(wxmsg.content);
            app_msg.conent.append(sentence);
            app_msg.timestamp = QDateTime::currentSecsSinceEpoch();
        } else {
            wxid = wxmsg.sender;
            auto& app_msg = msgList[wxid];
            app_msg.conent.append(wxmsg.content);
            app_msg.timestamp = QDateTime::currentSecsSinceEpoch();
        }
    }
}

void Application::onHandle()
{
    for (auto i = msgList.cbegin(), end = msgList.cend(); i != end; ++i) {
        const auto& wxid = i.key();
        const auto& app_msg = i.value();
        // 超过特定时间没有新消息就开始让机器人回复
        if (QDateTime::currentSecsSinceEpoch() - app_msg.timestamp > 10) {
            auto speaker = contactList[wxid];
            QString content;
            if (speaker.attr == QStringLiteral("群聊")) {
                content.append(QStringLiteral("在 %1 群聊里:\n").arg(speaker.name))
                    .append(app_msg.conent.join("\n"));
            } else {
                content.append(QStringLiteral("%1 说:\n").arg(speaker.name))
                    .append(app_msg.conent.join("\n"));
            }
            auto reply = chatRobot->talk(content);

            Request req = Request_init_default;
            req.func = Functions_FUNC_SEND_TXT;
            req.which_msg = Request_txt_tag;
            QByteArray receiver = wxid.toUtf8();
            QByteArray msg = reply.toUtf8();
            req.msg.txt.receiver = (char*)receiver.constData();
            req.msg.txt.msg = (char*)msg.constData();
            //sendRequest(req);
            LOG(debug) << reply;

            QMutexLocker locker(&mutex);
            msgList.remove(wxid);
            return; //处理完马上退出，因为修改了msgList本身
        }
    }
}
