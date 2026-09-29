#include "WsSecurity.h"

QString WsSecurity::generateHeader(const QString &username, const QString &password)
{
    QString nonce = generateNonce();
    QString created = generateCreated();
    QString digest = computeDigest(nonce, created, password);

    return QString(
        "<soap:Header>"
        "  <wsse:Security soap:mustUnderstand=\"1\""
        "      xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\""
        "      xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">"
        "    <wsse:UsernameToken>"
        "      <wsse:Username>%1</wsse:Username>"
        "      <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">%2</wsse:Password>"
        "      <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">%3</wsse:Nonce>"
        "      <wsu:Created>%4</wsu:Created>"
        "    </wsse:UsernameToken>"
        "  </wsse:Security>"
        "</soap:Header>"
    ).arg(username, digest, nonce, created);
}

QString WsSecurity::generateNonce()
{
    QByteArray nonce(16, 0);
    for (int i = 0; i < 16; ++i) {
        nonce[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return base64Encode(nonce);
}

QString WsSecurity::generateCreated()
{
    return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ");
}

QString WsSecurity::computeDigest(const QString &nonce, const QString &created, const QString &password)
{
    QByteArray nonceDecoded = QByteArray::fromBase64(nonce.toUtf8());
    QByteArray createdBytes = created.toUtf8();
    QByteArray passwordBytes = password.toUtf8();

    QByteArray combined = nonceDecoded + createdBytes + passwordBytes;
    QByteArray hash = sha1(combined);
    return base64Encode(hash);
}

QByteArray WsSecurity::base64Encode(const QByteArray &data)
{
    return data.toBase64();
}

QByteArray WsSecurity::sha1(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
}