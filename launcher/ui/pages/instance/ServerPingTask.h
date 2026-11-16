#pragma once

#include <QObject>
#include <QString>

#include <tasks/Task.h>

class ServerPingTask : public Task {
    Q_OBJECT
   public:
    explicit ServerPingTask(QString domain, int port) : Task(), m_domain(domain), m_port(port) {}
    ~ServerPingTask() override = default;
    int m_outputOnlinePlayers = -1;
    /// MOTD from the server's status response; formatting codes are stripped, plain text only
    QString m_outputMotd;

   private:
    QString m_domain;
    int m_port;

   protected:
    virtual void executeTask() override;
};
