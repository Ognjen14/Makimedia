#pragma once

#include "TmdbError.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>

namespace Makimedia::Tmdb
{

class TmdbJsonReader final
{
public:
    explicit TmdbJsonReader(QJsonObject object);

    [[nodiscard]] std::optional<std::int64_t> requiredInteger(
        const QString &fieldName);
    [[nodiscard]] std::optional<std::int64_t> optionalInteger(
        const QString &fieldName);
    [[nodiscard]] std::optional<double> requiredNumber(const QString &fieldName);
    [[nodiscard]] std::optional<double> optionalNumber(const QString &fieldName);
    [[nodiscard]] std::optional<std::string> requiredString(
        const QString &fieldName);
    [[nodiscard]] std::optional<std::string> optionalString(
        const QString &fieldName);
    [[nodiscard]] std::optional<bool> requiredBoolean(const QString &fieldName);
    [[nodiscard]] std::optional<bool> optionalBoolean(const QString &fieldName);
    [[nodiscard]] std::optional<QJsonArray> requiredArray(const QString &fieldName);
    [[nodiscard]] std::optional<QJsonArray> optionalArray(const QString &fieldName);
    [[nodiscard]] std::optional<QJsonObject> requiredObject(const QString &fieldName);
    [[nodiscard]] std::optional<QJsonObject> optionalObject(const QString &fieldName);

    [[nodiscard]] bool hasError() const noexcept;
    [[nodiscard]] const std::optional<TmdbError> &error() const noexcept;

private:
    enum class Requirement
    {
        Required,
        Optional
    };

    [[nodiscard]] std::optional<QJsonValue> value(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<std::int64_t> integer(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<double> number(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<std::string> string(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<bool> boolean(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<QJsonArray> array(
        const QString &fieldName,
        Requirement requirement);
    [[nodiscard]] std::optional<QJsonObject> object(
        const QString &fieldName,
        Requirement requirement);

    void setFieldError(const QString &fieldName);

    QJsonObject m_object;
    std::optional<TmdbError> m_error;
};

}
