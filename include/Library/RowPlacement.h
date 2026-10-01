#pragma once

#include <QList>

namespace RowPlacement {

enum class Kind {
    Nothing,
    Update,
    Insert,
    Remove,
    Move
};

struct Step
{
    Kind kind = Kind::Nothing;
    int from = -1;
    int to = -1;
};

template <typename T, typename Less>
int insertionRow(const QList<T> &rows, int skip, const T &value, Less less)
{
    int low = 0;
    int high = int(rows.size()) - (skip >= 0 ? 1 : 0);
    while (low < high) {
        const int middle = low + (high - low) / 2;
        const int actual = (skip >= 0 && middle >= skip) ? middle + 1 : middle;
        if (less(rows.at(actual), value)) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

template <typename T, typename Less>
Step plan(const QList<T> &rows, int current, const T &value, bool keep, Less less)
{
    Step step;
    if (!keep) {
        if (current >= 0) {
            step.kind = Kind::Remove;
            step.from = current;
        }
        return step;
    }

    const int target = insertionRow(rows, current, value, less);
    if (current < 0) {
        step.kind = Kind::Insert;
        step.to = target;
        return step;
    }

    step.from = current;
    step.to = target;
    step.kind = target == current ? Kind::Update : Kind::Move;
    return step;
}

}
