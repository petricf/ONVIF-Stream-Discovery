#ifndef ONVIFMODELS_H
#define ONVIFMODELS_H

#include <QString>
#include <QVector>
#include <QDomElement>
#include "StreamInfo.h"

class OnvifModels
{
public:
    static QString buildEnvelope(const QString &headerXml, const QString &bodyXml);

    // SOAP request bodies
    static QString getSystemDateAndTimeBody();
    static QString getCapabilitiesBody();
    static QString getProfilesBody();
    static QString getStreamUriBody(const QString &profileToken);
    static QString getSnapshotUriBody(const QString &profileToken);

    // Response parsing
    static bool parseMediaServiceUrl(const QString &xml, QString &mediaUrl);
    static QVector<ProfileInfo> parseProfiles(const QString &xml);
    static QString parseStreamUri(const QString &xml);
    static QString parseSnapshotUri(const QString &xml);
    static QString parseDeviceTime(const QString &xml);
    static QString extractFaultString(const QString &xml);

private:
    OnvifModels() = default;

    static VideoEncoderConfig parseVideoEncoderConfig(const QDomElement &vecElement);
};

#endif // ONVIFMODELS_H