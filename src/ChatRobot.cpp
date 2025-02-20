#include "ChatRobot.h"

ChatRobot::ChatRobot()
{
}

ChatRobot::~ChatRobot()
{
}

void ChatRobot::setPrompt(const QString& prompt)
{
}

QString ChatRobot::talk(const QString& content)
{
    return QStringLiteral("自动回复：\n")+content;
}
