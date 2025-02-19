#include "Application.h"
#include "Logger.h"
#include "sdk.h"
#include "NngSocket.h"
#include "DataUtil.h"
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

    startReceiveMessage();

    handleTimer = new QTimer(this);
    handleTimer->setInterval(1000);
    handleTimer->start();
    connect(handleTimer, &QTimer::timeout, this, &Application::onHandle);
}

Application::~Application()
{
    delete reqSocket;
    delete msgSocket;
}

void Application::startReceiveMessage()
{
    if (isReceiving) return;

    LOG(info) << QStringLiteral("开始接收微信消息...");
    Request req = Request_init_default;
    req.func = Functions_FUNC_ENABLE_RECV_TXT;
    req.which_msg = Request_func_tag;
    reqSocket->send(DataUtil::encode(req));
    msgSocket->connectToHost(NNG_HOST, nngPort + 1);

    isReceiving = true;
    QtConcurrent::run([this]() {
        while (isReceiving) {
            // 获取wx接收的信息
            auto rsp = DataUtil::toResponse(msgSocket->waitForRecv());
            // 可以在这里打断点调试，查看更多信息
            LOG(info) << (*rsp).msg.wxmsg.content;
        }
    });
}

void Application::stopReceiveMessage()
{
    LOG(info) << QStringLiteral("停止接收微信消息");
    isReceiving = false;

    Request req = Request_init_default;
    req.func = Functions_FUNC_DISABLE_RECV_TXT;
    req.which_msg = Request_func_tag;
    reqSocket->send(DataUtil::encode(req));
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

void Application::onHandle()
{
    //LOG(debug) << QStringLiteral("TODO：检测微信接收消息的队列");
}
