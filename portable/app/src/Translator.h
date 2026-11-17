// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Interface languages from the Windows build's catalog (i18n/catalog.json, keyed by the Simplified
// Chinese source text, the same rule as src/ui/UiLanguage.cpp): zh-CN shows the source, zh-TW and ja
// their own text, every other language falls back to English, never to Chinese.
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace pushfps {

class CatalogTranslator final : public QTranslator {
public:
    // code: "auto" (system language) or one of codes().
    bool load(const QString& code);
    QString code() const { return code_; }
    static QStringList codes();
    static QString resolve(const QString& preference);
    QString translate(const char* context, const char* source, const char* disambiguation, int n) const override;
    bool isEmpty() const override { return exact_.isEmpty(); }

private:
    QHash<QString, QString> exact_;
    QString code_ = QStringLiteral("zh-CN");
};

}  // namespace pushfps
