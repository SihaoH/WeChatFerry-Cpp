#pragma once
#include <QString>

class ChatRobot
{
public:
    ChatRobot();
    ~ChatRobot();

    void setPrompt(const QString& prompt);
    QString talk(const QString& content);
};
