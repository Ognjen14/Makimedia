#include "Metadata/TmdbJsonReader.h"

#include <cmath>
#include <limits>
#include <utility>

namespace Makimedia::Tmdb
{

TmdbJsonReader::TmdbJsonReader(QJsonObject object)
    : m_object(std::move(object))
{
}

std::optional<std::int64_t> TmdbJsonReader::requiredInteger(
    const QString &fieldName)
{
    return integer(fieldName, Requirement::Required);
}

std::optional<std::int64_t> TmdbJsonReader::optionalInteger(
    const QString &fieldName)
{
    return integer(fieldName, Requirement::Optional);
}

std::optional<double> TmdbJsonReader::requiredNumber(const QString &fieldName)
{
    return number(fieldName, Requirement::Required);
}

std::optional<double> TmdbJsonReader::optionalNumber(const QString &fieldName)
{
    return number(fieldName, Requirement::Optional);
}

std::optional<std::string> TmdbJsonReader::requiredString(
    const QString &fieldName)
{
    return string(fieldName, Requirement::Required);
}

std::optional<std::string> TmdbJsonReader::optionalString(
    const QString &fieldName)
{
    return string(fieldName, Requirement::Optional);
}

std::optional<bool> TmdbJsonReader::requiredBoolean(const QString &fieldName)
{
    return boolean(fieldName, Requirement::Required);
}

std::optional<bool> TmdbJsonReader::optionalBoolean(const QString &fieldName)
{
    return boolean(fieldName, Requirement::Optional);
}

std::optional<QJsonArray> TmdbJsonReader::requiredArray(const QString &fieldName)
{
    return array(fieldName, Requirement::Required);
}

std::optional<QJsonArray> TmdbJsonReader::optionalArray(const QString &fieldName)
{
    return array(fieldName, Requirement::Optional);
}

std::optional<QJsonObject> TmdbJsonReader::requiredObject(const QString &fieldName)
{
    return object(fieldName, Requirement::Required);
}

std::optional<QJsonObject> TmdbJsonReader::optionalObject(const QString &fieldName)
{
    return object(fieldName, Requirement::Optional);
}

bool TmdbJsonReader::hasError() const noexcept
{
    return m_error.has_value();
}

const std::optional<TmdbError> &TmdbJsonReader::error() const noexcept
{
    return m_error;
}

std::optional<QJsonValue> TmdbJsonReader::value(
    const QString &fieldName,
    Requirement requirement)
{
    if (hasError())
    {
        return std::nullopt;
    }

    const QJsonValue fieldValue = m_object.value(fieldName);
    if (fieldValue.isUndefined() || fieldValue.isNull())
    {
        if (requirement == Requirement::Required)
        {
            setFieldError(fieldName);
        }
        return std::nullopt;
    }

    return fieldValue;
}

std::optional<std::int64_t> TmdbJsonReader::integer(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isDouble())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    const double rawNumber = fieldValue->toDouble();
    const long double wideNumber = static_cast<long double>(rawNumber);
    const long double minimum = static_cast<long double>(
        std::numeric_limits<std::int64_t>::lowest());
    const long double maximum = static_cast<long double>(
        std::numeric_limits<std::int64_t>::max());

    if (!std::isfinite(rawNumber)
        || std::trunc(wideNumber) != wideNumber
        || wideNumber < minimum
        || wideNumber > maximum)
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return static_cast<std::int64_t>(rawNumber);
}

std::optional<double> TmdbJsonReader::number(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isDouble())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    const double rawNumber = fieldValue->toDouble();
    if (!std::isfinite(rawNumber))
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return rawNumber;
}

std::optional<std::string> TmdbJsonReader::string(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isString())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return fieldValue->toString().toUtf8().toStdString();
}

std::optional<bool> TmdbJsonReader::boolean(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isBool())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return fieldValue->toBool();
}

std::optional<QJsonArray> TmdbJsonReader::array(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isArray())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return fieldValue->toArray();
}

std::optional<QJsonObject> TmdbJsonReader::object(
    const QString &fieldName,
    Requirement requirement)
{
    const auto fieldValue = value(fieldName, requirement);
    if (!fieldValue.has_value())
    {
        return std::nullopt;
    }

    if (!fieldValue->isObject())
    {
        setFieldError(fieldName);
        return std::nullopt;
    }

    return fieldValue->toObject();
}

void TmdbJsonReader::setFieldError(const QString &fieldName)
{
    if (hasError())
    {
        return;
    }

    m_error.emplace(
        TmdbError::Category::Parse,
        QStringLiteral("TMDB returned invalid data for the '%1' field.")
            .arg(fieldName)
            .toUtf8()
            .toStdString());
}

}
