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
    QString talk(const QString& wxid, const QString& content);

private:
    std::string model = "llama3.2:1b";
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

QString ChatRobotPrivate::talk(const QString& wxid, const QString& content)
{
    QString reply;
    ollama::response context;
    if (contextMap.contains(wxid)) {
        context = ollama.generate(model, std::string(content.toUtf8()), contextMap[wxid]);
    } else {
        context = ollama.generate(model, std::string(content.toUtf8()), priContext);
    }
    contextMap[wxid] = context;
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

QString ChatRobot::talk(const QString& wxid, const QString& content)
{
    return p->talk(wxid, content);
}
