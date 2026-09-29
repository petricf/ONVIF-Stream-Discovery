#include "OnvifClient.h"
#include "WsSecurity.h"
#include "OnvifModels.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QEventLoop>
#include <QSslConfiguration>
#include <QDebug>

OnvifClient::OnvifClient(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

OnvifClient::~OnvifClient()
{
}

void OnvifClient::setCredentials(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;
}

void OnvifClient::setTimeout(int seconds)
{
    m_timeoutSeconds = seconds;
}

void OnvifClient::getSystemDateAndTime(const QString &deviceUrl, DeviceTimeCallback callback)
{
    QString body = OnvifModels::getSystemDateAndTimeBody();
    QString envelope = OnvifModels::buildEnvelope("", body);

    sendSoapRequest(deviceUrl, envelope,
        [callback](bool success, const QString &response) {
            if (success) {
                QString time = OnvifModels::parseDeviceTime(response);
                callback(true, time.isEmpty() ? "Could not parse device time" : time);
            } else {
                callback(false, response);
            }
        },
        "GetSystemDateAndTime");
}

void OnvifClient::getCapabilities(const QString &deviceUrl, MediaUrlCallback callback)
{
    QString header = WsSecurity::generateHeader(m_username, m_password);
    QString body = OnvifModels::getCapabilitiesBody();
    QString envelope = OnvifModels::buildEnvelope(header, body);

    sendSoapRequest(deviceUrl, envelope,
        [callback](bool success, const QString &response) {
            if (success) {
                QString mediaUrl;
                if (OnvifModels::parseMediaServiceUrl(response, mediaUrl)) {
                    callback(true, mediaUrl);
                } else {
                    callback(false, "No Media service XAddr found in response");
                }
            } else {
                callback(false, response);
            }
        },
        "GetCapabilities");
}

void OnvifClient::getProfiles(const QString &mediaUrl, ProfilesCallback callback)
{
    QString header = WsSecurity::generateHeader(m_username, m_password);
    QString body = OnvifModels::getProfilesBody();
    QString envelope = OnvifModels::buildEnvelope(header, body);

    sendSoapRequest(mediaUrl, envelope,
        [callback](bool success, const QString &response) {
            if (success) {
                QVector<ProfileInfo> profiles = OnvifModels::parseProfiles(response);
                if (profiles.isEmpty()) {
                    callback(false, QVector<ProfileInfo>(), "No profiles returned by device");
                } else {
                    callback(true, profiles, QString());
                }
            } else {
                callback(false, QVector<ProfileInfo>(), response);
            }
        },
        "GetProfiles");
}

void OnvifClient::getStreamUri(const QString &mediaUrl, const QString &profileToken, StreamUriCallback callback)
{
    QString header = WsSecurity::generateHeader(m_username, m_password);
    QString body = OnvifModels::getStreamUriBody(profileToken);
    QString envelope = OnvifModels::buildEnvelope(header, body);

    sendSoapRequest(mediaUrl, envelope,
        [callback](bool success, const QString &response) {
            if (success) {
                QString uri = OnvifModels::parseStreamUri(response);
                callback(true, uri, QString());
            } else {
                callback(false, QString(), response);
            }
        },
        QString("GetStreamUri(%1)").arg(profileToken));
}

void OnvifClient::getSnapshotUri(const QString &mediaUrl, const QString &profileToken, SnapshotUriCallback callback)
{
    QString header = WsSecurity::generateHeader(m_username, m_password);
    QString body = OnvifModels::getSnapshotUriBody(profileToken);
    QString envelope = OnvifModels::buildEnvelope(header, body);

    sendSoapRequest(mediaUrl, envelope,
        [callback](bool success, const QString &response) {
            if (success) {
                QString uri = OnvifModels::parseSnapshotUri(response);
                callback(true, uri, QString());
            } else {
                callback(false, QString(), response);
            }
        },
        QString("GetSnapshotUri(%1)").arg(profileToken));
}

void OnvifClient::sendSoapRequest(const QString &url, const QString &soapXml,
                                  std::function<void(bool, const QString&)> callback,
                                  const QString &description)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/soap+xml; charset=utf-8");
    request.setRawHeader("Content-Length", QByteArray::number(soapXml.toUtf8().length()));

    if (url.startsWith("https://", Qt::CaseInsensitive)) {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(sslConfig);
    }

    QNetworkReply *reply = m_networkManager->post(request, soapXml.toUtf8());

    PendingRequest pending;
    pending.callback = callback;
    pending.description = description;
    m_pendingRequests[reply] = pending;

    // Connect the reply's finished signal to our slot
    connect(reply, &QNetworkReply::finished, this, &OnvifClient::onReplyFinished);

    emit requestStarted(description);

    // Use QTimer::singleShot without context object
    QTimer::singleShot(m_timeoutSeconds * 1000, [this, reply]() {
        if (m_pendingRequests.contains(reply)) {
            reply->abort();
        }
    });
}

void OnvifClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_pendingRequests.contains(reply)) {
        return;
    }

    PendingRequest pending = m_pendingRequests.take(reply);
    bool success = (reply->error() == QNetworkReply::NoError);
    QString response = reply->readAll();

    if (!success) {
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error = QString("HTTP %1: %2").arg(httpCode).arg(reply->errorString());
        if (httpCode >= 400) {
            QString fault = OnvifModels::extractFaultString(response);
            if (!fault.isEmpty()) {
                error += " - " + fault;
            }
        }
        emit requestFinished(pending.description, false);
        pending.callback(false, error);
    } else {
        emit requestFinished(pending.description, true);
        pending.callback(true, response);
    }

    reply->deleteLater();
}