// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "Translator.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

namespace pushfps {

QStringList CatalogTranslator::codes() {
    return {QStringLiteral("zh-CN"), QStringLiteral("zh-TW"), QStringLiteral("en"), QStringLiteral("ja"),
            QStringLiteral("nl"), QStringLiteral("de"), QStringLiteral("es"), QStringLiteral("fr"),
            QStringLiteral("it"), QStringLiteral("pt-BR"), QStringLiteral("tr"), QStringLiteral("ar"),
            QStringLiteral("pl"), QStringLiteral("ru")};
}

QString CatalogTranslator::resolve(const QString& preference) {
    if (codes().contains(preference)) return preference;
    const QLocale system = QLocale::system();
    if (system.language() == QLocale::Chinese) {
        const auto territory = system.territory();
        const bool traditional = system.script() == QLocale::TraditionalChineseScript || territory == QLocale::Taiwan ||
                                 territory == QLocale::HongKong || territory == QLocale::Macao;
        return traditional ? QStringLiteral("zh-TW") : QStringLiteral("zh-CN");
    }
    switch (system.language()) {
    case QLocale::Japanese: return QStringLiteral("ja");
    case QLocale::Dutch: return QStringLiteral("nl");
    case QLocale::German: return QStringLiteral("de");
    case QLocale::Spanish: return QStringLiteral("es");
    case QLocale::French: return QStringLiteral("fr");
    case QLocale::Italian: return QStringLiteral("it");
    case QLocale::Portuguese: return QStringLiteral("pt-BR");
    case QLocale::Turkish: return QStringLiteral("tr");
    case QLocale::Arabic: return QStringLiteral("ar");
    case QLocale::Polish: return QStringLiteral("pl");
    case QLocale::Russian: return QStringLiteral("ru");
    default: return QStringLiteral("en");
    }
}

bool CatalogTranslator::load(const QString& preference) {
    code_ = resolve(preference);
    exact_.clear();
    if (code_ == QStringLiteral("zh-CN")) return true;
    QFile file(QStringLiteral(":/i18n/catalog.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const bool ownOnly = code_ == QStringLiteral("zh-TW") || code_ == QStringLiteral("ja");
    for (const auto value : doc.object().value(QStringLiteral("entries")).toArray()) {
        const auto e = value.toObject();
        const QString source = e.value(QStringLiteral("zh")).toString();
        QString translated = e.value(code_).toString();
        if (translated.isEmpty() && !ownOnly) translated = e.value(QStringLiteral("en")).toString();
        if (source.isEmpty() || translated.isEmpty()) continue;
        const QString context = e.value(QStringLiteral("ctx")).toString();
        exact_.insert(context.isEmpty() ? source : source + QChar(0x4) + context, translated);
    }
    return !exact_.isEmpty();
}

QString CatalogTranslator::translate(const char*, const char* source, const char* disambiguation, int) const {
    if (!source) return {};
    const QString key = QString::fromUtf8(source);
    if (disambiguation && *disambiguation) {
        const auto it = exact_.constFind(key + QChar(0x4) + QString::fromUtf8(disambiguation));
        if (it != exact_.constEnd()) return *it;
    }
    const auto it = exact_.constFind(key);
    return it == exact_.constEnd() ? QString() : *it;
}

}  // namespace pushfps
