#include "TextFold.h"

#include <utility>

namespace TextFold {

QString key(const QString &text)
{
    QString value = text.normalized(QString::NormalizationForm_KD);
    value.removeIf([](QChar c) { return c.category() == QChar::Mark_NonSpacing; });

    QString folded;
    folded.reserve(value.size());
    for (const QChar c : std::as_const(value)) {
        switch (c.unicode()) {
        case u'Đ':
        case u'đ':
            folded += QStringLiteral("dj");
            break;
        case u'Ł':
        case u'ł':
            folded += QLatin1Char('l');
            break;
        case u'Ø':
        case u'ø':
            folded += QLatin1Char('o');
            break;
        case u'ß':
            folded += QStringLiteral("ss");
            break;
        case u'Æ':
        case u'æ':
            folded += QStringLiteral("ae");
            break;
        case u'Œ':
        case u'œ':
            folded += QStringLiteral("oe");
            break;
        default:
            folded += c;
            break;
        }
    }

    return folded.toCaseFolded();
}

int compare(const QString &a, const QString &b)
{
    const int byKey = key(a).compare(key(b));
    return byKey != 0 ? byKey : a.compare(b);
}

}
