#include "Application.h"
#include "Logger.h"
#include "sdk.h"
#include "ChatRoBot.h"
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QThreadPool>
#include <QFileSystemWatcher>

#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>


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
    delete client;
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
                    // 端口只有初次打开时可以设置
                    nngPort = cfg_app.value("port").toInt();
                    initWCF();
                    initClient();
                }
                if (cfg_app.value("dumpfriends").toBool()) {
                    dumpFriendList();
                }
                waitTime = cfg_app.value("waittime").toInt();
            }

            auto cfg_chat = json_obj.value("chat").toObject();
            if (!cfg_chat.isEmpty()) {
                chatConfig.autoReply = cfg_chat.value("autoreply").toVariant().toStringList();
                chatConfig.onlyAter = cfg_chat.value("onlyater").toBool();
                LOG(info) << QString("自动回复的白名单：[%1]").arg(chatConfig.autoReply.join(", "));
                LOG(info) << QString("是否只在@时回复：%1").arg(chatConfig.onlyAter ? "是" : "否");
            }

            auto cfg_robot = json_obj.value("robot").toObject();
            if (!cfg_robot.isEmpty()) {
                if (chatRobot) {
                    delete chatRobot;
                }
                LOG(info) << "初始化聊天机器人：设置模型及投喂初始提示语...";
                chatRobot = new ChatRobot();
                chatRobot->setModel(cfg_robot.value("model").toString());
                chatRobot->setPrompt(cfg_robot.value("prompt").toString());
            }

            // 读取邀请配置
            auto cfg_invite = json_obj.value("invite").toObject();
            if (!cfg_invite.isEmpty()) {
                inviteConfig.keyword = cfg_invite.value("keyword").toString();
                inviteConfig.reply = cfg_invite.value("reply").toString();
                inviteConfig.roomId = cfg_invite.value("roomid").toString();
                LOG(info) << QString("邀请关键词：%1").arg(inviteConfig.keyword);
                LOG(info) << QString("邀请回复：%1").arg(inviteConfig.reply);
                LOG(info) << QString("目标群ID：%1").arg(inviteConfig.roomId);
            }

            auto cfg_admin = json_obj.value("admin").toObject();
            if (!cfg_admin.isEmpty()) {
                adminConfig.wxid = cfg_admin.value("wxid").toString();
                adminConfig.startCmd = cfg_admin.value("start").toString();
                adminConfig.stopCmd = cfg_admin.value("stop").toString();
                adminConfig.quitCmd = cfg_admin.value("quit").toString();
                LOG(info) << QString("管理员ID：%1").arg(adminConfig.wxid);
                LOG(info) << QString("管理指令：%1/%2/%3").arg(adminConfig.startCmd, adminConfig.stopCmd, adminConfig.quitCmd);
            }
        } else {
            LOG(err) << QString("错误：解析配置文件%1失败，请检查json语法").arg(configFile);
        }
    } else {
        LOG(err) << QString("错误：无法打开配置文件") + configFile;
    }
}

void Application::initWCF()
{
    if (isWeChatRunning()) {
        LOG(info) << "初始化WCF：微信正在运行，跳过注入步骤！";

    } else {
        LOG(info) << "初始化WCF：微信没有运行，启动微信并注入...";
        int ret = 0;
    #if _DEBUG
        ret = WxInitSDK(true, nngPort);
    #else
        ret = WxInitSDK(false, nngPort);
    #endif
        Q_ASSERT_X(ret == 0, "初始化WCF", "注入失败，请确认并重启程序!");
    }
}

void Application::initClient()
{
    client = new Client(this, nngPort);
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
    LOG(info) << "等待/确认用户登录...";
    while (!client->isLogin()) {
        QThread::sleep(1);
    }
}

void Application::dumpFriendList()
{
    LOG(info) << "拉取联系人清单...";
    auto list = client->getFriendList();

    // 将好友列表存文件
    QFile file("./friends.txt");
    if (file.open(QIODeviceBase::WriteOnly)) {
        file.resize(0);
        for (const auto& contact : list) {
            file.write(QString("%1 %2 %3\n").arg(contact.wxid).arg(contact.name).arg(contact.attr).toUtf8());
        }
    }
}

void Application::startReceiveMessage()
{
    if (isReceiving) return;

    LOG(info) << "开始接收微信消息...";
    client->setReceiveMessage(true);

    isReceiving = true;
    QThreadPool::globalInstance()->start([this]() {
        asyncReceiving();
    });
}

void Application::stopReceiveMessage()
{
    LOG(info) << "停止接收微信消息";
    isReceiving = false;
    client->setReceiveMessage(false);
    QThread::sleep(1);
}

void Application::asyncReceiving()
{
    while (isReceiving) {
        Client::Options opt;
        opt.types = { MsgType::Text, MsgType::Image, MsgType::Audio, MsgType::Video, MsgType::Refer };
        opt.onlyAter = chatConfig.onlyAter;
        auto msg = client->receiveMessage(opt);

        QMutexLocker locker(&mutex);
        QString wxid = msg.roomid.isEmpty() ? msg.sender : msg.roomid;
        msgMap[wxid].list.append(msg);
        msgMap[wxid].timestamp = QDateTime::currentSecsSinceEpoch();
    }
}

void Application::onHandle()
{
    for (auto i = msgMap.cbegin(), end = msgMap.cend(); i != end; ++i) {
        const auto& wxid = i.key();
        const auto& section = i.value();
        // 超过特定时间没有新消息才集中处理
        if (QDateTime::currentSecsSinceEpoch() - section.timestamp > waitTime) {
            // 处理管理员命令
            if (wxid == adminConfig.wxid) {
                QString word = section.list.first().content;
                if (word == adminConfig.startCmd) {
                    isAutoReplying = true;
                    client->sendText(wxid, "已开启自动回复");
                } else if (word == adminConfig.stopCmd) {
                    isAutoReplying = false;
                    client->sendText(wxid, "已停止自动回复");
                } else if (word == adminConfig.quitCmd) {
                    LOG(info) << "收到退出命令，程序即将关闭...";
                    QCoreApplication::quit();
                }
            } else {
                // 检查消息列表是否包含邀请关键词
                bool has_keyword = false;
                if (!section.list.first().roomid.isEmpty()) {
                    for (const auto& msg : section.list) {
                        if (msg.content.contains(inviteConfig.keyword)) {
                            has_keyword = true;
                            break;
                        }
                    }
                }
                if (has_keyword) {
                    client->sendText(wxid, inviteConfig.reply);
                    QThread::msleep(rand()%3000);
                    client->inviteRoomMembers(inviteConfig.roomId, wxid);
                } else if (isAutoReplying && chatConfig.autoReply.contains(wxid)) {
                    QString texts;
                    for (const auto& msg : section.list) {
                        if (msg.type == MsgType::Text) {
                            texts.append(QString("%1: %2\n").arg(msg.name).arg(msg.content));
                        }
                    }
                    auto reply = chatRobot->talk(wxid, texts);
                    client->sendText(wxid, reply);
                    //client->sendText(wxid, "@{wxid_i584qm1ofvdu22}哈哈哈哈");
                    //client->sendPatPat(wxid, "wxid_i584qm1ofvdu22");
                    //client->inviteRoomMembers(wxid, "wxid_i584qm1ofvdu22");
                } else {
                    QString texts;
                    for (const auto& msg : section.list) {
                        texts.append(msg.content + "\n");
                    }
                    LOG(debug) << "\n" << wxid << ": \n" << texts;
                }
            }

            QMutexLocker locker(&mutex);
            msgMap.remove(wxid);
            break; // 一次只处理一条消息
        }
    }
}
