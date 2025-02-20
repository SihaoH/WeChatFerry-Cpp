#include "Application.h"
#include "Logger.h"
#include "sdk.h"
#include "NngSocket.h"
#include "ChatRoBot.h"
#include <QTimer>
#include <QProcess>
#include <QFile>
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

    initWhiteList();
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
        LOG(info) << QStringLiteral("初始化WCF：微信正在运行，跳过注入步骤！");

    } else {
        LOG(info) << QStringLiteral("初始化WCF：微信没有运行，启动微信并注入...");
        int ret = 0;
    #if _DEBUG
        ret = WxInitSDK(true, nngPort);
    #else
        ret = WxInitSDK(false, nngPort);
    #endif
        Q_ASSERT_X(ret == 0, "初始化WCF", "注入失败，请确认并重启程序!");
    }
}

void Application::initNngClient()
{
    LOG(info) << QStringLiteral("初始化NNG：连接NNG服务端...");
    reqSocket = new NngSocket();
    msgSocket = new NngSocket();
    reqSocket->connectToHost(NNG_HOST, nngPort);
    reqSocket->setSendTimeout(5000);
    reqSocket->setRecvTimeout(500);
}

void Application::initChatRobot()
{
    LOG(info) << QStringLiteral("初始化聊天机器人：读取prompt文件内容喂给AI...");
    chatRobot = new ChatRobot;
    QFile config("./prompt");
    if (config.exists() && config.open(QIODeviceBase::ReadOnly)) {
        chatRobot->setPrompt(config.readAll());
    }
}

void Application::initWhiteList()
{
    LOG(info) << QStringLiteral("初始化自动回复的白名单：读取whitelist文件...");
    // 自动回复的白名单，若不在此则不回复
    QFile config("./whitelist");
    if (config.exists() && config.open(QIODeviceBase::ReadOnly)) {
        const QString content = config.readAll();
        whiteList.append(content.split("\r\n"));
    }
}

void Application::waitForLogin()
{
    LOG(info) << QStringLiteral("等待/确认用户登录...");
    Request req = Request_init_default;
    req.func = Functions_FUNC_IS_LOGIN;
    req.which_msg = Response_status_tag;
    for (;;) {
        auto rsp = sendRequest(req);
        if ((*rsp).msg.status > 0) {
            break;
        }
        QThread::sleep(1);
    }
}

void Application::pullContacts()
{
    LOG(info) << QStringLiteral("拉取联系人清单...");
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

    // 将联系人清单写入文件，方便后续操作
    QFile file("./contacts");
    if (file.open(QIODeviceBase::WriteOnly)) {
        file.resize(0);
        for (auto i = contactList.cbegin(), end = contactList.cend(); i != end; ++i) {
            file.write(QString("%1 %2\n").arg(i.key()).arg(i.value().name).toUtf8());
        }
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
        auto content = QString(wxmsg.content);
        if (content.startsWith("<msg>") || content.startsWith("<?xml version=\"1.0\"?>")) {
            // 特殊消息，先不处理
            continue;
        }

        QMutexLocker locker(&mutex);
        QString wxid;
        if (wxmsg.is_group) {
            if (wxmsg.is_self) {
                // 如果是群聊里面自己说的就不管
                continue;
            }
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
            if (whiteList.contains(wxid)) {
                Request req = Request_init_default;
                req.func = Functions_FUNC_SEND_TXT;
                req.which_msg = Request_txt_tag;
                QByteArray receiver = wxid.toUtf8();
                QByteArray msg = reply.toUtf8();
                req.msg.txt.receiver = (char*)receiver.constData();
                req.msg.txt.msg = (char*)msg.constData();
                sendRequest(req);
            } else {
                LOG(debug) << reply;
            }

            QMutexLocker locker(&mutex);
            msgList.remove(wxid);
            return; //处理完马上退出，因为修改了msgList本身
        }
    }
}
