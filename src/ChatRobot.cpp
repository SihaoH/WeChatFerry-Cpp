#include "ChatRobot.h"
#include "Logger.h"
#include <httplib.h>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

class ChatRobotPrivate
{
public:
    ChatRobotPrivate() : ollama(new httplib::Client("http://localhost:11434")) {}
    ~ChatRobotPrivate() = default;

    void setModel(const QString& _model);
    QString talk(const QString& wxid, const QString& content, const QStringList& images);

private:
    QString model = "qwen2.5:3b";
    QScopedPointer<httplib::Client> ollama;
    QMap<QString, QVariantList> contextMap;
};

void ChatRobotPrivate::setModel(const QString& _model)
{
    model = _model;
}

QString ChatRobotPrivate::talk(const QString& wxid, const QString& content, const QStringList& images)
{
    if (content.isEmpty() && images.isEmpty()) {
        return "发的是什么，我没看懂";
    }
    QString question;
    if (content.isEmpty() && !images.isEmpty()) {
        question = "请用中文解释";
    } else {
        question = content;
    }
    QString reply;
    
    QJsonObject requestObj;
    requestObj["model"] = model;
    requestObj["prompt"] = question;
    requestObj["stream"] = false;
    
    // 如果有上下文，添加到请求中
    if (contextMap.contains(wxid)) {
        requestObj["context"] = QJsonArray::fromVariantList(contextMap[wxid]);
    }
    
    // 如果有图片，添加到请求中
    if (!images.isEmpty()) {
        QJsonArray imageArray;
        for (const auto& img : images) {
            QFile file(img);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray imageData = file.readAll();
                imageArray.append(QString::fromLatin1(imageData.toBase64()));
            }
        }
        requestObj["images"] = imageArray;
    }
    
    QJsonDocument requestDoc(requestObj);
    auto res = ollama->Post("/api/generate", 
                          requestDoc.toJson(QJsonDocument::Compact).toStdString(),
                          "application/json");
    
    if (res && res->status == 200) {
        QJsonDocument responseDoc = QJsonDocument::fromJson(QByteArray::fromStdString(res->body));
        QJsonObject responseObj = responseDoc.object();
        
        reply = responseObj["response"].toString();
        
        // 保存上下文，但只在没有图片的情况下
        if (images.isEmpty() && responseObj.contains("context")) {
            contextMap[wxid] = responseObj["context"].toArray().toVariantList();
        }
    } else {
        reply = "抱歉，我遇到了一些问题，请稍后再试。";
    }
    
    return reply.trimmed();
}

ChatRobot::ChatRobot()
{
    p = new ChatRobotPrivate();
}

ChatRobot::~ChatRobot()
{
    delete p;
}

void ChatRobot::setModel(const QString& _model)
{
    p->setModel(_model);
}

QString ChatRobot::talk(const QString& wxid, const QString& content, const QStringList& images)
{
    return p->talk(wxid, content, images);
}
