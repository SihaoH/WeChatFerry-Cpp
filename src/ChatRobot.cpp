#include "ChatRobot.h"
#include "Logger.h"
#include "ollama.hpp"
#include <QRegularExpression>

class ChatRobotPrivate
{
public:
    ChatRobotPrivate() : ollama("http://localhost:11434") {}
    ~ChatRobotPrivate() = default;

    void setPrompt(const QString& prompt);
    void setModel(const QString& _model);
    QString talk(const QString& wxid, const QString& content, const QStringList& images);

private:
    std::string model = "qwen2.5:3b";
    Ollama ollama;
    ollama::response priContext;
    QMap<QString, ollama::response> contextMap;
};

void ChatRobotPrivate::setPrompt(const QString& prompt)
{
    priContext = ollama.generate(model, std::string(prompt.toUtf8()));
}

void ChatRobotPrivate::setModel(const QString& _model)
{
    model = std::string(_model.toUtf8());
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
    ollama::response context;
    std::vector<std::string> std_images;
    for (const auto& img : images) {
        std_images.push_back(ollama::image::from_file(std::string(img.toUtf8())));
    }
    ollama::options options;
    if (contextMap.contains(wxid)) {
        context = ollama.generate(model, std::string(question.toUtf8()), contextMap[wxid], options, std_images);
    } else {
        context = ollama.generate(model, std::string(question.toUtf8()), priContext, options, std_images);
    }
    // 如果记住了带有图片的上下文，则每次生成都需要带着那些图片数据
    if (std_images.empty()) {
        contextMap[wxid] = context;
    }
    reply = QString::fromStdString(context.as_simple_string()).trimmed();
    return reply;
}

ChatRobot::ChatRobot()
{
    p = new ChatRobotPrivate();
}

ChatRobot::~ChatRobot()
{
    delete p;
}

void ChatRobot::setPrompt(const QString& prompt)
{
    p->setPrompt(prompt);
}

void ChatRobot::setModel(const QString& _model)
{
    p->setModel(_model);
}

QString ChatRobot::talk(const QString& wxid, const QString& content, const QStringList& images)
{
    return p->talk(wxid, content, images);
}
