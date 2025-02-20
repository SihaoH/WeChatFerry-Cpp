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
    QString talk(const QString& content);

private:
    std::string model = "llama3.2:1b";
    Ollama ollama;
    ollama::response context;
};

void ChatRobotPrivate::setPrompt(const QString& prompt)
{
    context = ollama.generate(model, std::string(prompt.toUtf8()));
}

QString ChatRobotPrivate::talk(const QString& content)
{
    context = ollama.generate(model, std::string(content.toUtf8()), context);
    QString reply = QString::fromStdString(context.as_simple_string());
    reply = reply.trimmed();
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

QString ChatRobot::talk(const QString& content)
{
    return p->talk(content);
}
