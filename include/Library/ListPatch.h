#pragma once

#include <QHash>
#include <QList>
#include <QSet>

#include <algorithm>

namespace ListPatch {

enum class Kind {
    Remove,
    Insert,
    Move,
    Update
};

struct Step
{
    Kind kind = Kind::Update;
    int from = -1;
    int to = -1;
    int count = 1;
};

template <typename Key>
QList<Step> plan(const QList<Key> &oldKeys, const QList<Key> &newKeys)
{
    QList<Step> steps;

    QSet<Key> wanted;
    wanted.reserve(newKeys.size());
    for (const Key &key : newKeys) {
        wanted.insert(key);
    }

    QList<Key> work = oldKeys;

    for (int row = int(work.size()) - 1; row >= 0; --row) {
        if (wanted.contains(work.at(row))) {
            continue;
        }

        int first = row;
        while (first > 0 && !wanted.contains(work.at(first - 1))) {
            --first;
        }

        Step step;
        step.kind = Kind::Remove;
        step.from = first;
        step.count = row - first + 1;
        steps.append(step);

        work.remove(first, step.count);
        row = first;
    }

    QHash<Key, int> at;
    at.reserve(work.size());
    for (int row = 0; row < int(work.size()); ++row) {
        at.insert(work.at(row), row);
    }

    for (int target = 0; target < int(newKeys.size()); ++target) {
        const int row = at.value(newKeys.at(target), -1);

        if (row < 0) {
            int count = 1;
            while (target + count < int(newKeys.size())
                   && !at.contains(newKeys.at(target + count))) {
                ++count;
            }

            Step step;
            step.kind = Kind::Insert;
            step.to = target;
            step.count = count;
            steps.append(step);

            for (int i = 0; i < count; ++i) {
                work.insert(target + i, newKeys.at(target + i));
            }
            for (int r = target; r < int(work.size()); ++r) {
                at.insert(work.at(r), r);
            }

            target += count - 1;
            continue;
        }

        if (row != target) {
            Step step;
            step.kind = Kind::Move;
            step.from = row;
            step.to = target;
            steps.append(step);

            work.move(row, target);
            for (int r = target; r <= row; ++r) {
                at.insert(work.at(r), r);
            }
            continue;
        }

        Step step;
        step.kind = Kind::Update;
        step.to = target;
        steps.append(step);
    }

    return steps;
}

inline int structuralRows(const QList<Step> &steps)
{
    int rows = 0;
    for (const Step &step : steps) {
        if (step.kind != Kind::Update) {
            rows += step.count;
        }
    }
    return rows;
}

inline bool worthPatching(const QList<Step> &steps, int oldSize, int newSize)
{
    const int most = std::max(oldSize, newSize);
    return structuralRows(steps) <= std::max(8, most / 2);
}

}
