// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_other.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/integration.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {
namespace {
}

rpl::producer<QString> AyuOther::title() {
	return tr::ayu_CategoryOther();
}

AyuOther::AyuOther(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupCrashReporting(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSkip(container);
	AddSubsectionTitle(container, tr::ayu_CategoryOther());

	AddButtonWithIcon(
		container,
		tr::ayu_CrashReporting(),
		st::settingsButton,
		{&st::menuIconReport}
	)->toggleOn(
		rpl::single(settings->crashReporting)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->crashReporting);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_crashReporting(enabled);
			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_CrashReportingDescription());
}

void SetupOtherThings(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	AddSkip(container);
	AddButtonWithIcon(
		container,
		tr::ayu_RegisterURLScheme(),
		st::settingsButton,
		{&st::menuIconLink}
	)->setClickedCallback([=]
	{
		Core::Application::RegisterUrlScheme();
		controller->showToast(tr::lng_box_done(tr::now));
	});
	AddButtonWithIcon(
		container,
		tr::ayu_ResetSettings(),
		st::settingsButton,
		{&st::menuIconRestore}
	)->setClickedCallback([=]
	{
		controller->show(Ui::MakeConfirmBox({
			.text = tr::ayu_ResetSettingsConfirmation(tr::rich),
			.confirmed = [=](Fn<void()> &&close)
			{
				AyuSettings::reset();
				controller->showToast(tr::lng_box_done(tr::now));
				close();
			},
			.confirmText = tr::lng_box_yes(),
		}));
	});
	AddSkip(container);
}

void AyuOther::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	SetupCrashReporting(content);
#endif
	SetupOtherThings(content, controller);

	ResizeFitChild(this, content);
}

} // namespace Settings
