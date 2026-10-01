#pragma once

#include "Metadata/FileNameParser.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace TmdbAsk {

enum class Verdict {
    AskNow,
    AlreadyAsking,
    AlreadyAnswered
};

QString searchKey(const ParsedFileName &parsed, bool useYear);
QString detailsKey(qint64 tmdbId, const QString &kind);
QString seasonKey(qint64 tmdbId, int seasonNumber);
QString collectionKey(qint64 collectionId);

bool isDefinitive(const std::optional<int> &httpStatus);

template <typename Answer = bool, typename Waiter = bool>
class Ledger
{
public:
    Verdict ask(const QString &key, const Waiter &waiter = Waiter())
    {
        if (m_answers.contains(key)) {
            return Verdict::AlreadyAnswered;
        }
        const bool asking = m_waiting.contains(key);
        m_waiting[key].append(waiter);
        return asking ? Verdict::AlreadyAsking : Verdict::AskNow;
    }

    QList<Waiter> answer(const QString &key, const Answer &value = Answer())
    {
        m_answers.insert(key, value);
        return m_waiting.take(key);
    }

    QList<Waiter> fail(const QString &key, const std::optional<int> &httpStatus)
    {
        if (isDefinitive(httpStatus)) {
            m_answers.insert(key, Answer());
        }
        return m_waiting.take(key);
    }

    Answer answerFor(const QString &key) const { return m_answers.value(key); }
    bool isAnswered(const QString &key) const { return m_answers.contains(key); }
    bool isAsking(const QString &key) const { return m_waiting.contains(key); }

    void forgetAnswers() { m_answers.clear(); }

private:
    QHash<QString, Answer> m_answers;
    QHash<QString, QList<Waiter>> m_waiting;
};

}
