#pragma once
#include <QString>

class ChatRobot
{
public:
    ChatRobot();
    ~ChatRobot();

    void setPrompt(const QString& prompt);
    void setModel(const QString& _model);
    QString talk(const QString& wxid, const QString& content);

private:
    class ChatRobotPrivate* p = nullptr;
};
