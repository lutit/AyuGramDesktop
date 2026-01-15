// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "./base.h"

namespace Ayu::Translator {

class OpenAITranslator final : public MultiThreadTranslator {
	Q_OBJECT

public:
	static OpenAITranslator &instance();

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const MultiThreadArgs &args
	) override;

private:
	explicit OpenAITranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;
};

} // namespace Ayu::Translator
