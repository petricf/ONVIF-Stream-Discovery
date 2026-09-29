#ifndef STREAMINFO_H
#define STREAMINFO_H

#include <QString>
#include <QVector>

struct VideoEncoderConfig {
    QString encoding;
    int width = 0;
    int height = 0;
    int fps = 0;
    int bitrateKbps = 0;
    int govLength = 0;
    QString codecProfile;
    QString quality;

    bool isValid() const { return width > 0 && height > 0; }
};

struct ProfileInfo {
    QString token;
    QString name;
    VideoEncoderConfig video;
};

struct StreamResult {
    QString profileName;
    QString rtspUri;
    QString rtspUriRaw;
    QString snapshotUri;
    VideoEncoderConfig video;
};

#endif // STREAMINFO_H