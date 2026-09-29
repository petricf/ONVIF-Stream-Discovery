#include <QCoreApplication>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QTranslator>
#include <QLocale>
#include "MainWindow.h"
#include "OnvifClient.h"
#include "StreamInfo.h"

class CliRunner : public QObject
{
    Q_OBJECT

public:
    CliRunner(const QString &ip, const QString &user, const QString &pass,
              int port, const QString &path, bool https, int timeout)
        : m_ip(ip), m_user(user), m_pass(pass), m_port(port), m_path(path),
          m_https(https), m_timeout(timeout)
    {
        m_client = new OnvifClient(this);
        connect(m_client, &OnvifClient::errorOccurred, this, &CliRunner::onError);
    }

    void run()
    {
        if (!m_path.startsWith('/')) {
            m_path = '/' + m_path;
        }

        m_deviceUrl = QString("%1://%2:%3%4")
            .arg(m_https ? "https" : "http")
            .arg(m_ip)
            .arg(m_port)
            .arg(m_path);

        qDebug() << tr("== ONVIF stream discovery for %1 ==").arg(m_ip);
        qDebug() << tr("Device service: %1").arg(m_deviceUrl);
        qDebug() << "";

        m_client->setCredentials(m_user, m_pass);
        m_client->setTimeout(m_timeout);

        // Step 1: Get device time (unauthenticated)
        qDebug() << tr("-- Checking device reachability --");
        m_client->getSystemDateAndTime(m_deviceUrl,
            [this](bool success, const QString &result) {
                onDeviceTimeResult(success, result);
            });
    }

private slots:
    void onDeviceTimeResult(bool success, const QString &result)
    {
        if (success && !result.isEmpty() && !result.startsWith("Could not")) {
            qDebug() << tr("  Device UTC time: %1").arg(result);
        } else {
            qDebug() << tr("  (could not read device time: %1)").arg(result);
        }
        qDebug() << "";

        // Step 2: Get capabilities
        qDebug() << tr("-- GetCapabilities (locating Media service) --");
        m_client->getCapabilities(m_deviceUrl,
            [this](bool success, const QString &result) {
                onCapabilitiesResult(success, result);
            });
    }

    void onCapabilitiesResult(bool success, const QString &result)
    {
        if (success) {
            m_mediaUrl = result;
            qDebug() << tr("  Media service XAddr: %1").arg(m_mediaUrl);
            qDebug() << "";

            // Step 3: Get profiles
            qDebug() << tr("-- GetProfiles --");
            m_client->getProfiles(m_mediaUrl,
                [this](bool success, const QVector<ProfileInfo> &profiles, const QString &error) {
                    onProfilesResult(success, profiles, error);
                });
        } else {
            qCritical() << tr("GetCapabilities failed: %1").arg(result);
            QCoreApplication::exit(1);
        }
    }

    void onProfilesResult(bool success, const QVector<ProfileInfo> &profiles, const QString &error)
    {
        if (success) {
            m_profiles = profiles;
            if (profiles.isEmpty()) {
                qCritical() << tr("No media profiles returned by the device.");
                QCoreApplication::exit(1);
                return;
            }

            for (const auto &p : profiles) {
                qDebug() << tr("  Profile token: %1  (name: %2)").arg(p.token, p.name);
                qDebug() << tr("    Video: %1").arg(formatVideoInfo(p.video));
            }
            qDebug() << "";

            // Step 4 & 5: Get stream and snapshot URIs for each profile
            qDebug() << tr("-- GetStreamUri / GetSnapshotUri per profile --");
            m_pendingRequests = profiles.size() * 2;
            m_streamResults.clear();
            m_streamResults.resize(profiles.size());

            for (int i = 0; i < profiles.size(); ++i) {
                const ProfileInfo &profile = profiles[i];
                qDebug() << tr("  [%1] token=%2").arg(profile.name, profile.token);

                m_client->getStreamUri(m_mediaUrl, profile.token,
                    [this, i, profile](bool success, const QString &uri, const QString &error) {
                        onStreamUriResult(profile.token, success, uri, error);
                    });

                m_client->getSnapshotUri(m_mediaUrl, profile.token,
                    [this, i, profile](bool success, const QString &uri, const QString &error) {
                        onSnapshotUriResult(profile.token, success, uri, error);
                    });
            }
        } else {
            qCritical() << "GetProfiles failed:" << error;
            QCoreApplication::exit(1);
        }
    }

    void onStreamUriResult(const QString &profileToken, bool success, const QString &uri, const QString &error)
    {
        int profileIndex = -1;
        for (int i = 0; i < m_profiles.size(); ++i) {
            if (m_profiles[i].token == profileToken) {
                profileIndex = i;
                break;
            }
        }

        if (profileIndex >= 0) {
            StreamResult &result = m_streamResults[profileIndex];
            result.profileName = m_profiles[profileIndex].name;
            result.video = m_profiles[profileIndex].video;

            if (success && !uri.isEmpty()) {
                result.rtspUriRaw = uri;
                result.rtspUri = withCredentials(uri);
                qDebug() << tr("    RTSP:     %1").arg(maskUrl(result.rtspUri));
                qDebug() << tr("    (raw, no creds): %1").arg(uri);
            } else {
                qDebug() << tr("    RTSP:     not returned (%1)").arg(error);
            }
        }

        checkRequestsComplete();
    }

    void onSnapshotUriResult(const QString &profileToken, bool success, const QString &uri, const QString &)
    {
        int profileIndex = -1;
        for (int i = 0; i < m_profiles.size(); ++i) {
            if (m_profiles[i].token == profileToken) {
                profileIndex = i;
                break;
            }
        }

        if (profileIndex >= 0) {
            StreamResult &result = m_streamResults[profileIndex];

            if (success && !uri.isEmpty()) {
                result.snapshotUri = withCredentials(uri);
                qDebug() << tr("    Snapshot: %1").arg(maskUrl(result.snapshotUri));
            }
        }

        checkRequestsComplete();
    }

    void checkRequestsComplete()
    {
        m_pendingRequests--;
        if (m_pendingRequests <= 0) {
            printSummary();
            QCoreApplication::exit(0);
        }
    }

    void onError(const QString &message)
    {
        qCritical() << tr("ERROR: %1").arg(message);
        QCoreApplication::exit(1);
    }

private:
    void printSummary()
    {
        qDebug() << "";
        qDebug() << tr("== Summary ==");

        bool anyResults = false;
        for (const StreamResult &result : m_streamResults) {
            if (!result.rtspUri.isEmpty()) {
                anyResults = true;
                qDebug() << tr("- %1:").arg(result.profileName);
                qDebug() << tr("    RTSP:     %1").arg(maskUrl(result.rtspUri));
                if (!result.snapshotUri.isEmpty()) {
                    qDebug() << tr("    Snapshot: %1").arg(maskUrl(result.snapshotUri));
                }
                qDebug() << tr("    %1").arg(formatVideoInfo(result.video));
            }
        }

        if (!anyResults) {
            qDebug() << tr("No RTSP stream URIs were returned. Double-check credentials and that");
            qDebug() << tr("the ONVIF service path/port are correct for this device.");
        } else {
            qDebug() << "";
            qDebug() << tr("Use these URLs directly in VLC, ffmpeg, or your NVR/recording software.");
        }
    }

    QString maskUrl(const QString &url)
    {
        QUrl u(url);
        if (!u.password().isEmpty()) {
            u.setPassword("***");
        }
        return u.toString();
    }

    QString formatVideoInfo(const VideoEncoderConfig &video)
    {
        if (!video.isValid()) {
            return tr("(no VideoEncoderConfiguration in profile - device may not expose it here)");
        }

        QStringList parts;
        if (video.width > 0 && video.height > 0) {
            parts << QString("%1x%2").arg(video.width).arg(video.height);
        }
        if (video.fps > 0) {
            parts << tr("%1 fps").arg(video.fps);
        }
        if (!video.encoding.isEmpty()) {
            QString codec = video.encoding;
            if (!video.codecProfile.isEmpty()) {
                codec += tr(" (%1)").arg(video.codecProfile);
            }
            parts << codec;
        }
        if (video.bitrateKbps > 0) {
            parts << tr("%1 kbps").arg(video.bitrateKbps);
        }
        if (video.govLength > 0) {
            parts << tr("GOV %1").arg(video.govLength);
        }
        if (!video.quality.isEmpty()) {
            parts << tr("quality %1").arg(video.quality);
        }

        return parts.isEmpty() ? tr("(VideoEncoderConfiguration present but no fields populated)") : parts.join(", ");
    }

    QString withCredentials(const QString &uri)
    {
        QUrl u(uri);
        if (u.isValid() && !u.host().isEmpty()) {
            u.setUserName(QUrl::toPercentEncoding(m_user));
            u.setPassword(QUrl::toPercentEncoding(m_pass));
        }
        return u.toString();
    }

    OnvifClient *m_client = nullptr;
    QString m_ip;
    QString m_user;
    QString m_pass;
    int m_port = 80;
    QString m_path = "/onvif/device_service";
    bool m_https = false;
    int m_timeout = 5;
    QString m_deviceUrl;
    QString m_mediaUrl;
    QVector<ProfileInfo> m_profiles;
    QVector<StreamResult> m_streamResults;
    int m_pendingRequests = 0;
};

int main(int argc, char *argv[])
{
    // We need to check for CLI mode before creating the app
    // Check raw arguments for CLI indicators
    bool cliMode = false;
    QStringList rawArgs;
    for (int i = 1; i < argc; ++i) {
        rawArgs << QString::fromLocal8Bit(argv[i]);
    }

    for (const QString &arg : rawArgs) {
        if (arg == "--cli" || arg == "-cli") {
            cliMode = true;
            break;
        }
    }

    // Check if positional arguments are provided (PHP-style usage)
    QStringList positionalArgs;
    for (const QString &arg : rawArgs) {
        if (!arg.startsWith('-')) {
            positionalArgs << arg;
        }
    }
    if (positionalArgs.size() >= 3) {
        cliMode = true;
    }

    if (cliMode) {
        // Use QCoreApplication for CLI mode
        QCoreApplication app(argc, argv);
        app.setApplicationName("ONVIF Stream Discovery");
        app.setApplicationVersion("1.1.0");
        app.setOrganizationName("ONVIFTools");

        QTranslator translator;
        if (translator.load(":/onvif_stream_discover_de.qm")) {
            app.installTranslator(&translator);
        }

        QCommandLineParser parser;
        parser.setApplicationDescription("ONVIF Stream Discovery Tool - Qt6 Version");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption cliOption("cli", "Run in CLI mode (no GUI)");
        parser.addOption(cliOption);

        QCommandLineOption ipOption(QStringList() << "i" << "ip", "Camera IP address", "ip");
        parser.addOption(ipOption);

        QCommandLineOption userOption(QStringList() << "u" << "user", "Username", "username");
        parser.addOption(userOption);

        QCommandLineOption passOption(QStringList() << "p" << "password", "Password", "password");
        parser.addOption(passOption);

        QCommandLineOption portOption(QStringList() << "P" << "port", "Port (default: 80)", "port", "80");
        parser.addOption(portOption);

        QCommandLineOption pathOption("path", "Service path (default: /onvif/device_service)", "path", "/onvif/device_service");
        parser.addOption(pathOption);

        QCommandLineOption timeoutOption("timeout", "Timeout in seconds (default: 5)", "seconds", "5");
        parser.addOption(timeoutOption);

        QCommandLineOption httpsOption("https", "Use HTTPS");
        parser.addOption(httpsOption);

        parser.process(app);

        QString ip, user, pass;
        int port = 80;
        QString path = "/onvif/device_service";
        bool https = false;
        int timeout = 5;

        if (positionalArgs.size() >= 3) {
            // PHP-style: ip user pass [options]
            ip = positionalArgs[0];
            user = positionalArgs[1];
            pass = positionalArgs[2];
        } else {
            // Named options style
            ip = parser.value(ipOption);
            user = parser.value(userOption);
            pass = parser.value(passOption);
        }

        port = parser.value(portOption).toInt();
        path = parser.value(pathOption);
        timeout = parser.value(timeoutOption).toInt();
        https = parser.isSet(httpsOption);

        if (ip.isEmpty() || user.isEmpty() || pass.isEmpty()) {
            qCritical() << "Error: IP, username, and password are required.";
            parser.showHelp(1);
        }

        CliRunner runner(ip, user, pass, port, path, https, timeout);
        QTimer::singleShot(0, &runner, &CliRunner::run);
        return app.exec();
    } else {
        // Use QApplication for GUI mode
        QApplication app(argc, argv);
        app.setApplicationName("ONVIF Stream Discovery");
        app.setApplicationVersion("1.1.0");
        app.setOrganizationName("ONVIFTools");

        QTranslator translator;
        if (translator.load(":/onvif_stream_discover_de.qm")) {
            app.installTranslator(&translator);
        }

        QCommandLineParser parser;
        parser.setApplicationDescription("ONVIF Stream Discovery Tool - Qt6 Version");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption cliOption("cli", "Run in CLI mode (no GUI)");
        parser.addOption(cliOption);

        QCommandLineOption ipOption(QStringList() << "i" << "ip", "Camera IP address", "ip");
        parser.addOption(ipOption);

        QCommandLineOption userOption(QStringList() << "u" << "user", "Username", "username");
        parser.addOption(userOption);

        QCommandLineOption passOption(QStringList() << "p" << "password", "Password", "password");
        parser.addOption(passOption);

        QCommandLineOption portOption(QStringList() << "P" << "port", "Port (default: 80)", "port", "80");
        parser.addOption(portOption);

        QCommandLineOption pathOption("path", "Service path (default: /onvif/device_service)", "path", "/onvif/device_service");
        parser.addOption(pathOption);

        QCommandLineOption timeoutOption("timeout", "Timeout in seconds (default: 5)", "seconds", "5");
        parser.addOption(timeoutOption);

        QCommandLineOption httpsOption("https", "Use HTTPS");
        parser.addOption(httpsOption);

        parser.process(app);

        MainWindow window;
        window.show();
        return app.exec();
    }
}

#include "main.moc"