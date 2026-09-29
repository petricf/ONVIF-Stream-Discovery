#ifndef WSSECURITY_H
#define WSSECURITY_H

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>

class WsSecurity
{
public:
    static QString generateHeader(const QString &username, const QString &password);
    static QString generateNonce();
    static QString generateCreated();
    static QString computeDigest(const QString &nonce, const QString &created, const QString &password);
    static QByteArray base64Encode(const QByteArray &data);
    static QByteArray sha1(const QByteArray &data);

private:
    WsSecurity() = default;
};

#endif // WSSECURITY_H