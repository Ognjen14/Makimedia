#pragma once

#include "Data/MediaRepository.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class SearchIndex
{
public:
    using Credit = QPair<qint64, CreditRecord>;

    enum Tier {
        NoMatch = 0,
        Typo = 1,
        Contains = 2,
        Initials = 3,
        WordPrefix = 4,
        Prefix = 5,
        Exact = 6
    };

    struct TitleHit
    {
        MediaRecord title;
        int score = 0;
        QString person;
        int personKind = -1;
        QString role;
    };

    struct PersonHit
    {
        QString name;
        QString profilePath;
        bool acted = false;
        bool directed = false;
        bool created = false;
        QList<qint64> mediaIds;
        int tier = NoMatch;
    };

    void setTitles(const QList<MediaRecord> &titles);
    void setCredits(const QList<Credit> &credits);

    int titleCount() const;

    QList<TitleHit> titles(const QString &query, int limit = -1) const;
    QList<PersonHit> people(const QString &query, int limit = -1) const;

    QList<TitleHit> titlesOf(const QString &name) const;
    PersonHit person(const QString &name) const;

    static QString fold(const QString &text);

    static int distance(const QString &a, const QString &b, int most);

    static int titleScore(int tier);
    static int personScore(int tier);

private:
    struct Text
    {
        QString key;
        QString compact;
        QStringList tails;
        QString initials;
    };

    struct Query
    {
        QString key;
        QString compact;
        QStringList words;

        bool isEmpty() const { return compact.isEmpty(); }
    };

    struct Entry
    {
        MediaRecord record;
        Text title;
        Text original;
    };

    struct Role
    {
        int kind = CreditRecord::Cast;
        QString role;
        int order = 0;
    };

    struct Person
    {
        QString name;
        Text text;
        QString profilePath;
        QHash<qint64, QList<Role>> roles;
    };

    static Text textOf(const QString &value);
    static Query queryOf(const QString &value);
    static int match(const Text &text, const Query &query, bool initials);
    static bool startsAnyTail(const QString &queryWord, const QStringList &tails);
    static bool nearAnyTail(const QString &queryWord, const QStringList &tails);

    PersonHit hitOf(const Person &person, int tier) const;
    const Role *leadRole(const Person &person, qint64 mediaId) const;

    QList<Entry> m_entries;
    QHash<qint64, int> m_entryById;
    QList<Person> m_people;
    QHash<QString, int> m_personByKey;
};
