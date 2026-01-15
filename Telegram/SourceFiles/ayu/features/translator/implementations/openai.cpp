// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "openai.h"

#include <cmath>
#include <memory>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "ayu/ayu_settings.h"
#include "ayu/features/translator/html_parser.h"

namespace Ayu::Translator {

namespace {

QUrl BuildOpenAIUrl(const QString &baseUrl) {
	QUrl url(baseUrl);
	if (!url.isValid() || url.scheme().isEmpty()) {
		return {};
	}
	auto path = url.path();
	if (!path.endsWith(QStringLiteral("/chat/completions"))) {
		if (!path.endsWith('/')) {
			path.append('/');
		}
		path.append(QStringLiteral("chat/completions"));
		url.setPath(path);
	}
	return url;
}

QString BuildSystemPrompt(const AyuSettings::AyuGramSettings &settings,
						  const QString &fromLang,
						  const QString &toLang) {
	const auto source = (fromLang.trimmed().isEmpty() || fromLang == QStringLiteral("auto"))
		? QStringLiteral("the source language")
		: fromLang.trimmed();
	const auto target = toLang.trimmed();
	auto prompt = settings.openaiSystemPrompt.trimmed();
	if (prompt.isEmpty()) {
		prompt = QStringLiteral(
			"You are a translation engine. Translate text from {source_lang} to {target_lang}. "
			"Preserve meaning, formatting, line breaks, and any HTML tags. "
			"Reply with the translation only.");
	}
	prompt.replace(QStringLiteral("{source_lang}"), source);
	prompt.replace(QStringLiteral("{target_lang}"), target);
	prompt.replace(QStringLiteral("{source}"), source);
	prompt.replace(QStringLiteral("{target}"), target);
	return prompt;
}

void AddDoubleParam(QJsonObject &obj, const char *key, double value) {
	if (std::isfinite(value)) {
		obj.insert(QString::fromLatin1(key), value);
	}
}

} // namespace

OpenAITranslator &OpenAITranslator::instance() {
	static OpenAITranslator inst;
	return inst;
}

OpenAITranslator::OpenAITranslator(QObject *parent)
	: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> OpenAITranslator::startSingleTranslation(
	const MultiThreadArgs &args
) {
	const auto &text = args.parsedData.text;
	const auto &fromLang = args.parsedData.fromLang;
	const auto &toLang = args.parsedData.toLang;
	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;

	if (text.empty() || toLang.isEmpty()) {
		if (onFail) onFail();
		return nullptr;
	}

	const auto &settings = AyuSettings::getInstance();
	const auto token = settings.openaiApiKey.trimmed();
	const auto model = settings.openaiModel.trimmed();
	auto baseUrl = settings.openaiApiBaseUrl.trimmed();
	if (baseUrl.isEmpty()) {
		baseUrl = QStringLiteral("https://api.openai.com/v1");
	}
	const auto url = BuildOpenAIUrl(baseUrl);
	if (!url.isValid() || token.isEmpty() || model.isEmpty()) {
		if (onFail) onFail();
		return nullptr;
	}

	const auto prompt = BuildSystemPrompt(settings, fromLang, toLang);
	const auto bodyText = shouldWrapInHtml()
		? Html::entitiesToHtml(text)
		: text.text;

	QJsonObject root;
	root.insert(QStringLiteral("model"), model);
	QJsonArray messages;
	messages.append(QJsonObject{
		{ QStringLiteral("role"), QStringLiteral("system") },
		{ QStringLiteral("content"), prompt },
	});
	messages.append(QJsonObject{
		{ QStringLiteral("role"), QStringLiteral("user") },
		{ QStringLiteral("content"), bodyText },
	});
	root.insert(QStringLiteral("messages"), messages);

	AddDoubleParam(root, "temperature", settings.openaiTemperature);
	AddDoubleParam(root, "top_p", settings.openaiTopP);
	AddDoubleParam(root, "presence_penalty", settings.openaiPresencePenalty);
	AddDoubleParam(root, "frequency_penalty", settings.openaiFrequencyPenalty);
	if (settings.openaiMaxTokens > 0) {
		root.insert(QStringLiteral("max_tokens"), settings.openaiMaxTokens);
	}

	const auto payload = QJsonDocument(root).toJson(QJsonDocument::Compact);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));

	auto authHeader = settings.openaiAuthHeader.trimmed();
	if (authHeader.isEmpty()) {
		authHeader = QStringLiteral("Authorization");
	}
	auto authPrefix = settings.openaiAuthPrefix.trimmed();
	if (!authPrefix.isEmpty() && !authPrefix.endsWith(' ')) {
		authPrefix.append(' ');
	}
	req.setRawHeader(authHeader.toUtf8(), (authPrefix + token).toUtf8());

	QPointer<QNetworkReply> reply = _nam.post(req, payload);

	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(15000);
	QObject::connect(timer,
					 &QTimer::timeout,
					 reply,
					 [reply]
					 {
						 if (!reply) return;
						 if (reply->isRunning()) reply->abort();
					 });
	timer->start();

	QObject::connect(reply,
					 &QNetworkReply::finished,
					 [reply, onSuccess = onSuccess, onFail = onFail, timer]
					 {
						 if (!reply) return;
						 timer->stop();
						 const auto guard = std::unique_ptr<QNetworkReply, void(*)(QNetworkReply *)>(
							 reply,
							 [](QNetworkReply *r) { r->deleteLater(); });

						 if (reply->error() != QNetworkReply::NoError) {
							 if (onFail) onFail();
							 return;
						 }

						 const auto body = reply->readAll();
						 bool ok = false;
						 auto translatedText = parseJsonPath(
							 body,
							 QStringLiteral("choices[0].message.content"),
							 &ok);
						 if (!ok) {
							 translatedText = parseJsonPath(
								 body,
								 QStringLiteral("choices[0].text"),
								 &ok);
						 }
						 if (!ok || translatedText.trimmed().isEmpty()) {
							 if (onFail) onFail();
							 return;
						 }

						 if (onSuccess) onSuccess(shouldWrapInHtml()
													  ? Html::htmlToEntities(translatedText)
													  : TextWithEntities{translatedText});
					 });

	return reply;
}

} // namespace Ayu::Translator
