#include <QFutureWatcher>
#include <QRegularExpression>

#include <Json.h>
#include "Exception.h"
#include "McClient.h"
#include "McResolver.h"
#include "ServerPingTask.h"

unsigned getOnlinePlayers(QJsonObject data)
{
    try {
        return Json::requireInteger(Json::requireObject(data, "players"), "online");
    } catch (Exception& e) {
        qWarning() << "server ping failed to parse response" << e.what();
        return 0;
    }
}

/// MOTD is a Minecraft chat component (string, object or array) - extract the plain text recursively
static QString extractChatText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        QString text = object.value("text").toString();
        if (object.value("extra").isArray()) {
            for (const auto& part : object.value("extra").toArray()) {
                text += extractChatText(part);
            }
        }
        return text;
    }
    if (value.isArray()) {
        QString text;
        for (const auto& part : value.toArray()) {
            text += extractChatText(part);
        }
        return text;
    }
    return {};
}

static QString stripFormattingCodes(const QString& text)
{
    static const QRegularExpression s_formattingCodes(QStringLiteral("\u00a7."));  // the section sign (U+00A7), legacy color codes
    QString stripped = text;
    stripped.remove(s_formattingCodes);
    // chat components use newlines for the two-line MOTD layout; keep them as newlines
    return stripped.trimmed();
}

static QString getMotd(QJsonObject data)
{
    return stripFormattingCodes(extractChatText(data.value("description")));
}

void ServerPingTask::executeTask()
{
    qDebug() << "Querying status of" << QString("%1:%2").arg(m_domain).arg(m_port);

    // Resolve the actual IP and port for the server
    McResolver* resolver = new McResolver(nullptr, m_domain, m_port);
    connect(resolver, &McResolver::succeeded, this, [this](QString ip, int port) {
        qDebug().nospace().noquote() << "Resolved address for " << m_domain << ": " << ip << ":" << port;

        // Now that we have the IP and port, query the server
        McClient* client = new McClient(nullptr, m_domain, ip, port);

        connect(client, &McClient::succeeded, this, [this](QJsonObject data) {
            m_outputOnlinePlayers = getOnlinePlayers(data);
            m_outputMotd = getMotd(data);
            qDebug() << "Online players:" << m_outputOnlinePlayers;
            emitSucceeded();
        });
        connect(client, &McClient::failed, this, [this](QString error) { emitFailed(error); });

        // Delete McClient object when done
        connect(client, &McClient::finished, this, [client]() { client->deleteLater(); });
        client->getStatusData();
    });
    connect(resolver, &McResolver::failed, this, [this](QString error) { emitFailed(error); });

    // Delete McResolver object when done
    connect(resolver, &McResolver::finished, [resolver]() { resolver->deleteLater(); });
    resolver->ping();
}
