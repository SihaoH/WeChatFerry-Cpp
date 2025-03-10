#pragma once
#include <QString>
#include <QStringList>

class ChatRobot
{
public:
    ChatRobot();
    ~ChatRobot();

    void setModel(const QString& _model);
    QString talk(const QString& wxid, const QString& content, const QStringList& images = QStringList());

private:
    class ChatRobotPrivate* p = nullptr;
};
