#include "Streaming/HttpMessage.h"

#include <QUrl>

namespace {

qint64 toCount(const QByteArray &text, bool *ok)
{
    const QByteArray trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        *ok = false;
        return 0;
    }
    for (const char c : trimmed) {
        if (c < '0' || c > '9') {
            *ok = false;
            return 0;
        }
    }
    return trimmed.toLongLong(ok);
}

}

namespace HttpMessage {

Parse takeRequest(QByteArray &buffer, HttpRequest &request)
{
    const int headEnd = int(buffer.indexOf("\r\n\r\n"));
    if (headEnd < 0) {
        return buffer.size() > MaxHeadBytes ? Parse::Bad : Parse::NeedMore;
    }
    if (headEnd > MaxHeadBytes) {
        return Parse::Bad;
    }

    const QList<QByteArray> lines = buffer.left(headEnd).split('\n');
    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.size() != 3 || !requestLine.at(2).startsWith("HTTP/1.")) {
        return Parse::Bad;
    }

    HttpRequest parsed;
    parsed.method = requestLine.at(0).toUpper();

    const QByteArray target = requestLine.at(1);
    if (!target.startsWith('/')) {
        return Parse::Bad;
    }
    const int questionMark = int(target.indexOf('?'));
    const QByteArray rawPath = questionMark < 0 ? target : target.left(questionMark);
    parsed.query = questionMark < 0 ? QByteArray() : target.mid(questionMark + 1);
    parsed.path = QUrl::fromPercentEncoding(rawPath);

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int colon = int(line.indexOf(':'));
        if (colon <= 0) {
            return Parse::Bad;
        }
        parsed.headers.insert(line.left(colon).trimmed().toLower(),
                              line.mid(colon + 1).trimmed());
    }

    const QByteArray connection = parsed.header("connection").toLower();
    if (requestLine.at(2) == "HTTP/1.0") {
        parsed.keepAlive = connection.contains("keep-alive");
    } else {
        parsed.keepAlive = !connection.contains("close");
    }

    qint64 bodyLength = 0;
    const QByteArray contentLength = parsed.header("content-length");
    if (!contentLength.isEmpty()) {
        bool ok = false;
        bodyLength = toCount(contentLength, &ok);
        if (!ok || bodyLength > MaxBodyBytes) {
            return Parse::Bad;
        }
    }

    const qint64 total = headEnd + 4 + bodyLength;
    if (buffer.size() < total) {
        return Parse::NeedMore;
    }

    parsed.body = buffer.mid(headEnd + 4, int(bodyLength));
    buffer.remove(0, int(total));
    request = parsed;
    return Parse::Ready;
}

ByteRange parseRange(const QByteArray &header, qint64 size)
{
    ByteRange range;
    range.end = size - 1;

    const QByteArray value = header.trimmed();
    if (!value.toLower().startsWith("bytes=")) {
        return range;
    }

    QByteArray spec = value.mid(6);
    const int comma = int(spec.indexOf(','));
    if (comma >= 0) {
        spec = spec.left(comma);
    }
    const int dash = int(spec.indexOf('-'));
    if (dash < 0) {
        return range;
    }

    const QByteArray from = spec.left(dash).trimmed();
    const QByteArray to = spec.mid(dash + 1).trimmed();

    bool ok = false;
    if (from.isEmpty()) {
        const qint64 suffix = toCount(to, &ok);
        if (!ok) {
            return range;
        }
        range.requested = true;
        if (suffix == 0 || size == 0) {
            range.satisfiable = false;
            return range;
        }
        range.start = qMax<qint64>(0, size - suffix);
        range.end = size - 1;
        return range;
    }

    const qint64 start = toCount(from, &ok);
    if (!ok) {
        return range;
    }
    qint64 end = size - 1;
    if (!to.isEmpty()) {
        end = toCount(to, &ok);
        if (!ok || end < start) {
            return range;
        }
    }

    range.requested = true;
    if (start >= size) {
        range.satisfiable = false;
        return range;
    }
    range.start = start;
    range.end = qMin(end, size - 1);
    return range;
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 206: return "Partial Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 416: return "Range Not Satisfiable";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}

QByteArray head(int status, const QList<QPair<QByteArray, QByteArray>> &headers)
{
    QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    for (const auto &header : headers) {
        out += header.first + ": " + header.second + "\r\n";
    }
    out += "\r\n";
    return out;
}

}
