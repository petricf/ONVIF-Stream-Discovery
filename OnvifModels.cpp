#include "OnvifModels.h"
#include <QDomDocument>
#include <QDomNodeList>
#include <QDomElement>
#include <QDomNamedNodeMap>

QString OnvifModels::buildEnvelope(const QString &headerXml, const QString &bodyXml)
{
    return QString(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<soap:Envelope"
        "    xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
        "    xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\""
        "    xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\""
        "    xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "%1"
        "  <soap:Body>"
        "%2"
        "  </soap:Body>"
        "</soap:Envelope>"
    ).arg(headerXml, bodyXml);
}

QString OnvifModels::getSystemDateAndTimeBody()
{
    return "<tds:GetSystemDateAndTime/>";
}

QString OnvifModels::getCapabilitiesBody()
{
    return "<tds:GetCapabilities><tds:Category>Media</tds:Category></tds:GetCapabilities>";
}

QString OnvifModels::getProfilesBody()
{
    return "<trt:GetProfiles/>";
}

QString OnvifModels::getStreamUriBody(const QString &profileToken)
{
    return QString(
        "<trt:GetStreamUri>"
        "  <trt:StreamSetup>"
        "    <tt:Stream>RTP-Unicast</tt:Stream>"
        "    <tt:Transport>"
        "      <tt:Protocol>RTSP</tt:Protocol>"
        "    </tt:Transport>"
        "  </trt:StreamSetup>"
        "  <trt:ProfileToken>%1</trt:ProfileToken>"
        "</trt:GetStreamUri>"
    ).arg(profileToken);
}

QString OnvifModels::getSnapshotUriBody(const QString &profileToken)
{
    return QString(
        "<trt:GetSnapshotUri>"
        "  <trt:ProfileToken>%1</trt:ProfileToken>"
        "</trt:GetSnapshotUri>"
    ).arg(profileToken);
}

bool OnvifModels::parseMediaServiceUrl(const QString &xml, QString &mediaUrl)
{
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return false;
    }

    QDomNodeList nodes = doc.elementsByTagName("tt:XAddr");
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement elem = nodes.at(i).toElement();
        QDomNode parent = elem.parentNode();
        if (parent.toElement().tagName() == "tt:Media") {
            mediaUrl = elem.text().trimmed();
            return true;
        }
    }
    return false;
}

VideoEncoderConfig OnvifModels::parseVideoEncoderConfig(const QDomElement &vecElement)
{
    VideoEncoderConfig config;

    QDomNodeList encodingNodes = vecElement.elementsByTagName("tt:Encoding");
    if (encodingNodes.count() > 0) {
        config.encoding = encodingNodes.at(0).toElement().text().trimmed();
    }

    QDomNodeList widthNodes = vecElement.elementsByTagName("tt:Width");
    if (widthNodes.count() > 0) {
        config.width = widthNodes.at(0).toElement().text().toInt();
    }

    QDomNodeList heightNodes = vecElement.elementsByTagName("tt:Height");
    if (heightNodes.count() > 0) {
        config.height = heightNodes.at(0).toElement().text().toInt();
    }

    QDomNodeList fpsNodes = vecElement.elementsByTagName("tt:FrameRateLimit");
    if (fpsNodes.count() > 0) {
        config.fps = fpsNodes.at(0).toElement().text().toInt();
    }

    QDomNodeList bitrateNodes = vecElement.elementsByTagName("tt:BitrateLimit");
    if (bitrateNodes.count() > 0) {
        config.bitrateKbps = bitrateNodes.at(0).toElement().text().toInt();
    }

    QDomNodeList govNodes = vecElement.elementsByTagName("tt:GovLength");
    if (govNodes.count() > 0) {
        config.govLength = govNodes.at(0).toElement().text().toInt();
    }

    QDomNodeList profileNodes = vecElement.elementsByTagName("tt:H264Profile");
    if (profileNodes.count() == 0) {
        profileNodes = vecElement.elementsByTagName("tt:H265Profile");
    }
    if (profileNodes.count() > 0) {
        config.codecProfile = profileNodes.at(0).toElement().text().trimmed();
    }

    QDomNodeList qualityNodes = vecElement.elementsByTagName("tt:Quality");
    if (qualityNodes.count() > 0) {
        config.quality = qualityNodes.at(0).toElement().text().trimmed();
    }

    return config;
}

QVector<ProfileInfo> OnvifModels::parseProfiles(const QString &xml)
{
    QVector<ProfileInfo> profiles;
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return profiles;
    }

    QDomNodeList profileNodes = doc.elementsByTagName("trt:Profiles");
    for (int i = 0; i < profileNodes.count(); ++i) {
        QDomElement profileElem = profileNodes.at(i).toElement();
        ProfileInfo profile;
        profile.token = profileElem.attribute("token");

        QDomNodeList nameNodes = profileElem.elementsByTagName("tt:Name");
        if (nameNodes.count() > 0) {
            profile.name = nameNodes.at(0).toElement().text().trimmed();
        } else {
            profile.name = "(unnamed)";
        }

        QDomNodeList vecNodes = profileElem.elementsByTagName("tt:VideoEncoderConfiguration");
        if (vecNodes.count() > 0) {
            profile.video = parseVideoEncoderConfig(vecNodes.at(0).toElement());
        }

        profiles.append(profile);
    }

    return profiles;
}

QString OnvifModels::parseStreamUri(const QString &xml)
{
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return QString();
    }

    QDomNodeList nodes = doc.elementsByTagName("tt:Uri");
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement elem = nodes.at(i).toElement();
        QDomNode parent = elem.parentNode();
        if (parent.toElement().tagName() == "trt:MediaUri") {
            return elem.text().trimmed();
        }
    }
    return QString();
}

QString OnvifModels::parseSnapshotUri(const QString &xml)
{
    return parseStreamUri(xml);
}

QString OnvifModels::parseDeviceTime(const QString &xml)
{
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return QString();
    }

    QDomNodeList dateNodes = doc.elementsByTagName("tt:Date");
    QDomNodeList timeNodes = doc.elementsByTagName("tt:Time");

    if (dateNodes.count() == 0 || timeNodes.count() == 0) {
        return QString();
    }

    QDomElement dateElem = dateNodes.at(0).toElement();
    QDomElement timeElem = timeNodes.at(0).toElement();

    QString year = dateElem.elementsByTagName("tt:Year").at(0).toElement().text();
    QString month = dateElem.elementsByTagName("tt:Month").at(0).toElement().text();
    QString day = dateElem.elementsByTagName("tt:Day").at(0).toElement().text();

    QString hour = timeElem.elementsByTagName("tt:Hour").at(0).toElement().text();
    QString minute = timeElem.elementsByTagName("tt:Minute").at(0).toElement().text();
    QString second = timeElem.elementsByTagName("tt:Second").at(0).toElement().text();

    return QString("%1-%2-%3 %4:%5:%6").arg(year, month, day, hour, minute, second);
}

QString OnvifModels::extractFaultString(const QString &xml)
{
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return QString();
    }

    QDomNodeList faultNodes = doc.elementsByTagName("soap:Fault");
    for (int i = 0; i < faultNodes.count(); ++i) {
        QDomElement faultElem = faultNodes.at(i).toElement();
        QDomNodeList textNodes = faultElem.elementsByTagName("Text");
        if (textNodes.count() > 0) {
            return textNodes.at(0).toElement().text().trimmed();
        }
        QDomNodeList reasonNodes = faultElem.elementsByTagName("soap:Reason");
        if (reasonNodes.count() > 0) {
            QDomNodeList textChildren = reasonNodes.at(0).toElement().elementsByTagName("soap:Text");
            if (textChildren.count() > 0) {
                return textChildren.at(0).toElement().text().trimmed();
            }
        }
    }
    return QString();
}