#include "Application.h"
#include "Logger.h"
#include "sdk.h"
#include "NngSocket.h"
#include "ChatRoBot.h"
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QtConcurrent>
#include <QFileSystemWatcher>

#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

constexpr const char* NNG_HOST = "tcp://127.0.0.1";

Application::Application(int& argc, char** argv)
    : QCoreApplication(argc, argv)
{
    Logger::instance()->init("app");

    reloadConfig(true);
    initHandler();
    initConfigWatcher();
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

void Application::reloadConfig(bool is_first)
{
    QFile file(configFile);
    if (file.exists() && file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError parse_err;
        auto json_doc = QJsonDocument::fromJson(file.readAll(), &parse_err);
        if (parse_err.error == QJsonParseError::NoError) {
            auto json_obj = json_doc.object();

            auto cfg_app = json_obj.value("app").toObject();
            if (!cfg_app.isEmpty()) {
                if (is_first) {
                    nngPort = cfg_app.value("port").toInt();
                    initWCF();
                    initNngClient();
                }
                if (cfg_app.value("pullcontacts").toBool()) {
                    pullContacts();
                }
            }

            auto cfg_chat = json_obj.value("chat").toObject();
            if (!cfg_chat.isEmpty()) {
                whiteList = cfg_chat.value("whitelist").toVariant().toStringList();
                waitTime = cfg_chat.value("waittime").toInt();
                LOG(info) << QStringLiteral("自动回复的白名单：[%1]").arg(whiteList.join(", "));
                LOG(info) << QStringLiteral("自动回复的等待时间：%1s").arg(waitTime);
            }

            auto cfg_robot = json_obj.value("robot").toObject();
            if (!cfg_robot.isEmpty()) {
                if (chatRobot) {
                    delete chatRobot;
                }
                chatRobot = new ChatRobot();
                chatRobot->setModel(cfg_robot.value("model").toString());
                chatRobot->setPrompt(cfg_robot.value("prompt").toString());
            }
        } else {
            LOG(err) << QStringLiteral("错误：解析配置文件%1失败，请检查json语法").arg(configFile);
        }
    } else {
        LOG(err) << QStringLiteral("错误：无法打开配置文件") + configFile;
    }
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

void Application::initHandler()
{
    waitForLogin();
    startReceiveMessage();
    handleTimer = new QTimer(this);
    handleTimer->setInterval(1000);
    handleTimer->start();
    connect(handleTimer, &QTimer::timeout, this, &Application::onHandle);
}

void Application::initConfigWatcher()
{
    configWatcher = new QFileSystemWatcher(this);
    configWatcher->addPath(configFile);
    connect(configWatcher, &QFileSystemWatcher::fileChanged, [this](const QString&) { reloadConfig(); });
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
    QString list;
    for (const auto& contact : contacts) {
        auto wxid = QString::fromStdString(contact.wxid);
        if (wxid.isEmpty() || wxid.startsWith("gh_")) {
            // 公众号不处理
            continue;
        }

        auto name = QString::fromStdString(contact.remark.empty() ? contact.name : contact.remark);
        if (name.isEmpty()) name = "<null>";
        QString attr;
        if (wxid.endsWith("@chatroom")) {
            attr = QStringLiteral("群聊");
        } else {
            if (contact.gender == 1) {
                attr = QStringLiteral("男");
            } else if(contact.gender == 2) {
                attr = QStringLiteral("女");
            } else {
                attr = QStringLiteral("个人");
            }
        }
        list.append(QString("%1 %2 %3\n").arg(wxid).arg(name).arg(attr));
    }

    // 将联系人清单写入文件，方便后续操作
    QFile file("./contacts");
    if (file.open(QIODeviceBase::WriteOnly)) {
        file.resize(0);
        file.write(list.toUtf8());
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

        QString wxid;
        if (wxmsg.is_group) {
            if (wxmsg.is_self) {
                // 如果是群聊里面自己说的就不管
                continue;
            }
            wxid = wxmsg.roomid;
        } else {
            wxid = wxmsg.sender;
        }

        QMutexLocker locker(&mutex);
        msgList[wxid].conent.append(content);
        msgList[wxid].timestamp = QDateTime::currentSecsSinceEpoch();
    }
}

void Application::onHandle()
{
    for (auto i = msgList.cbegin(), end = msgList.cend(); i != end; ++i) {
        const auto& wxid = i.key();
        const auto& app_msg = i.value();
        // 超过特定时间没有新消息就开始让机器人回复
        if (QDateTime::currentSecsSinceEpoch() - app_msg.timestamp > waitTime) {
            QString content = app_msg.conent.join("。\n");
            auto reply = chatRobot->talk(wxid, content);
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
                LOG(debug) << content << " => " << reply;
            }

            QMutexLocker locker(&mutex);
            msgList.remove(wxid);
            return; //处理完马上退出，因为修改了msgList本身
        }
    }
}
