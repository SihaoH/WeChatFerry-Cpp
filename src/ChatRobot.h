#pragma once
#include <QObject>

class ChatRobot : public QObject
{
    Q_OBJECT
public:
    ChatRobot(QObject* parent);
    ~ChatRobot();

    void setContacts();
    void setPrompt(const QString& prompt);
    void tell(const QString& wxid, const QString& content);

signals:
    void aboutToReply(const QString& wxid, const QString& content);
};
