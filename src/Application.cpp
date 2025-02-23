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
                if (cfg_app.value("pullcontacts").toBool()) {
                    pullContacts();
                }
                autoDelImg = cfg_app.value("autodelimg").toBool();
            }

            auto cfg_chat = json_obj.value("chat").toObject();
            if (!cfg_chat.isEmpty()) {
                whiteList = cfg_chat.value("whitelist").toVariant().toStringList();
                waitTime = cfg_chat.value("waittime").toInt();
                LOG(info) << QString("自动回复的白名单：[%1]").arg(whiteList.join(", "));
                LOG(info) << QString("自动回复的等待时间：%1s").arg(waitTime);
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

void Application::pullContacts()
{
    LOG(info) << "拉取联系人清单...";
    auto list = client->getContacts();

    // 将联系人清单写入文件，方便后续操作
    QFile file("./contacts");
    if (file.open(QIODeviceBase::WriteOnly)) {
        file.resize(0);
        for (const auto& contact : list) {
            file.write(QString("%1 %2 %3").arg(contact.wxid).arg(contact.name).arg(contact.attr).toUtf8());
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
}

void Application::asyncReceiving()
{
    while (isReceiving) {
        auto msg = client->receiveMessage();

        QMutexLocker locker(&mutex);
        msgMap[msg.sender].list.append(msg);
        msgMap[msg.sender].timestamp = QDateTime::currentSecsSinceEpoch();
    }
}

void Application::onHandle()
{
    QString rm_wxid;
    for (auto i = msgMap.cbegin(), end = msgMap.cend(); i != end; ++i) {
        const auto& wxid = i.key();
        const auto& section = i.value();
        // 超过特定时间没有新消息就开始让机器人回复
        if (QDateTime::currentSecsSinceEpoch() - section.timestamp > waitTime) {
            QString texts;
            QStringList images;
            for (const auto& msg : section.list) {
                if (msg.type == MsgType::Text) {
                    texts.append(msg.content + "\n");
                } else if (msg.type == MsgType::Image) {
                    images.append(msg.content);
                }
            }
            if (whiteList.contains(wxid)) {
                auto reply = chatRobot->talk(wxid, texts, images);
                client->sendText(wxid, reply);
            } else {
                LOG(debug) << wxid << ": " << texts;
            }

            if (autoDelImg) { // 自动删除保存的图片
                for (const auto& img : images) {
                    QFile::remove(img);
                }
            }

            rm_wxid = wxid;
            break; // 一次只处理一条消息
        }
    }

    if (!rm_wxid.isEmpty()) {
        QMutexLocker locker(&mutex);
        msgMap.remove(rm_wxid);
    }
}
