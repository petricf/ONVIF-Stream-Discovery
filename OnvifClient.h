#ifndef ONVIFCLIENT_H
#define ONVIFCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QVector>
#include <functional>
#include "StreamInfo.h"

class OnvifClient : public QObject
{
    Q_OBJECT

public:
    explicit OnvifClient(QObject *parent = nullptr);
    ~OnvifClient();

    void setCredentials(const QString &username, const QString &password);
    void setTimeout(int seconds);

    using DeviceTimeCallback = std::function<void(bool success, const QString &timeOrError)>;
    using MediaUrlCallback = std::function<void(bool success, const QString &mediaUrlOrError)>;
    using ProfilesCallback = std::function<void(bool success, const QVector<ProfileInfo> &profiles, const QString &error)>;
    using StreamUriCallback = std::function<void(bool success, const QString &streamUri, const QString &error)>;
    using SnapshotUriCallback = std::function<void(bool success, const QString &snapshotUri, const QString &error)>;

    void getSystemDateAndTime(const QString &deviceUrl, DeviceTimeCallback callback);
    void getCapabilities(const QString &deviceUrl, MediaUrlCallback callback);
    void getProfiles(const QString &mediaUrl, ProfilesCallback callback);
    void getStreamUri(const QString &mediaUrl, const QString &profileToken, StreamUriCallback callback);
    void getSnapshotUri(const QString &mediaUrl, const QString &profileToken, SnapshotUriCallback callback);

signals:
    void requestStarted(const QString &description);
    void requestFinished(const QString &description, bool success);
    void errorOccurred(const QString &message);

private slots:
    void onReplyFinished();

private:
    void sendSoapRequest(const QString &url, const QString &soapXml,
                         std::function<void(bool, const QString&)> callback,
                         const QString &description);

    QNetworkAccessManager *m_networkManager = nullptr;
    QString m_username;
    QString m_password;
    int m_timeoutSeconds = 5;

    struct PendingRequest {
        std::function<void(bool, const QString&)> callback;
        QString description;
    };
    QMap<QNetworkReply*, PendingRequest> m_pendingRequests;
};

#endif // ONVIFCLIENT_H