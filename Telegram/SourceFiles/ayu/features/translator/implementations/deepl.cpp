// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "deepl.h"

#include <memory>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "ayu/ayu_settings.h"
#include "ayu/features/translator/html_parser.h"

namespace Ayu::Translator {

namespace {

QUrl BuildDeepLUrl(const QString &baseUrl) {
	QUrl url(baseUrl);
	if (!url.isValid() || url.scheme().isEmpty()) {
		return {};
	}
	auto path = url.path();
	if (!path.endsWith(QStringLiteral("/translate"))) {
		if (!path.endsWith('/')) {
			path.append('/');
		}
		path.append(QStringLiteral("translate"));
		url.setPath(path);
	}
	return url;
}

QString NormalizeLang(const QString &code) {
	return code.trimmed().toUpper();
}

} // namespace

DeepLTranslator &DeepLTranslator::instance() {
	static DeepLTranslator inst;
	return inst;
}

DeepLTranslator::DeepLTranslator(QObject *parent)
	: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> DeepLTranslator::startSingleTranslation(
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
	const auto authKey = settings.deeplApiKey().trimmed();
	auto baseUrl = settings.deeplApiBaseUrl().trimmed();
	if (baseUrl.isEmpty()) {
		baseUrl = QStringLiteral("https://api-free.deepl.com/v2");
	}

	const auto url = BuildDeepLUrl(baseUrl);
	if (!url.isValid() || authKey.isEmpty()) {
		if (onFail) onFail();
		return nullptr;
	}

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader,
				  QStringLiteral("application/x-www-form-urlencoded"));
	req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
	req.setRawHeader(
		QByteArrayLiteral("Authorization"),
		QByteArray("DeepL-Auth-Key ") + authKey.toUtf8());

	QUrlQuery postData;
	const auto bodyText = shouldWrapInHtml()
		? Html::entitiesToHtml(text)
		: text.text;
	postData.addQueryItem(QStringLiteral("text"), bodyText);
	postData.addQueryItem(QStringLiteral("target_lang"), NormalizeLang(toLang));
	const auto from = fromLang.trimmed();
	if (!from.isEmpty() && from != QStringLiteral("auto")) {
		postData.addQueryItem(QStringLiteral("source_lang"), NormalizeLang(from));
	}
	if (shouldWrapInHtml()) {
		postData.addQueryItem(QStringLiteral("tag_handling"), QStringLiteral("html"));
	}
	postData.addQueryItem(QStringLiteral("preserve_formatting"), QStringLiteral("1"));
	const auto postDataEncoded = postData.toString(QUrl::FullyEncoded).toUtf8();

	QPointer<QNetworkReply> reply = _nam.post(req, postDataEncoded);

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
						 const auto translatedText = parseJsonPath(body, QStringLiteral("translations[0].text"), &ok);
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
