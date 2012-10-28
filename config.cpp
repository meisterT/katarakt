#include "config.h"
#include <QHashIterator>


CFG::CFG() :
		settings(QSettings::IniFormat, QSettings::UserScope, "katarakt") {
	// TODO warn about invalid user input

	settings.beginGroup("Settings");
	// canvas options
	defaults["mouse_wheel_factor"] = 120; // (qt-)delta for turning the mouse wheel 1 click
	defaults["smooth_scroll_delta"] = 30; // pixel scroll offset
	// layout options
	defaults["useless_gap"] = 2;
	defaults["min_page_width"] = 150;
	defaults["min_zoom"] = -14;
	defaults["max_zoom"] = 30;
	defaults["zoom_factor"] = 0.05;
	defaults["prefetch_count"] = 3;
	defaults["search_padding"] = 0.2; // must be <= 0.5
	// resourcemanager options
	defaults["smooth_downscaling"] = true; // filter when creating thumbnail image
	defaults["thumbnail_size"] = 32;
	// search options
	defaults["rect_expansion"] = 2;
	settings.endGroup();

	settings.beginGroup("keys");
	// canvas keys
	keys["set_presentation_layout"] = QStringList() << "1";
	keys["set_grid_layout"] = QStringList() << "2";
	keys["page_down"] = QStringList() << "Space" << "PgDown" << "Down";
	keys["page_up"] = QStringList() << "Backspace" << "PgUp" << "Up";
	keys["page_first"] = QStringList() << "G";
	keys["page_last"] = QStringList() << "Shift+G";
	keys["focus_goto"] = QStringList() << "Ctrl+G";
	keys["auto_smooth_up"] = QStringList() << "K";
	keys["auto_smooth_down"] = QStringList() << "J";
	keys["smooth_left"] = QStringList() << "H";
	keys["smooth_right"] = QStringList() << "L";
	keys["zoom_in"] = QStringList() << "=" << "+";
	keys["zoom_out"] = QStringList() << "-";
	keys["reset_zoom"] = QStringList() << "Z";
	keys["columns_inc"] = QStringList() << "]";
	keys["columns_dec"] = QStringList() << "[";
	keys["toggle_overlay"] = QStringList() << "T";
	keys["quit"] = QStringList() << "Q" << "W,E,E,E";
	keys["search"] = QStringList() << "/";
	keys["next_hit"] = QStringList() << "N";
	keys["previous_hit"] = QStringList() << "Shift+N";
	keys["rotate_left"] = QStringList() << "U";
	keys["rotate_right"] = QStringList() << "I";
	// viewer keys
	keys["toggle_fullscreen"] = QStringList() << "F";
	keys["close_search"] = QStringList() << "Esc";
	keys["reload"] = QStringList() << "R";
	settings.endGroup();
}

CFG::CFG(const CFG &/*other*/) {
}

CFG &CFG::operator=(const CFG &/*other*/) {
	return *this;
}

CFG::~CFG() {
	// do not write temporary options to disk
	settings.remove("start_page");
	settings.remove("fullscreen");

//	set_defaults(); // uncomment to create ini file with all the default settings
}

void CFG::set_defaults() {
	settings.beginGroup("Settings");
	QHashIterator<QString,QVariant> it(defaults);
	while (it.hasNext()) {
		it.next();
		settings.setValue(it.key(), it.value());
	}
	settings.endGroup();

	settings.beginGroup("Keys");
	QHashIterator<QString,QStringList> i2(keys);
	while (i2.hasNext()) {
		i2.next();
		settings.setValue(i2.key(), i2.value());
	}
	settings.endGroup();
}

CFG *CFG::get_instance() {
	static CFG instance;
	return &instance;
}

QVariant CFG::get_value(const char *key) const {
	return settings.value(QString("Settings/") + key, defaults[key]);
}

void CFG::set_value(const char *key, QVariant value) {
	settings.setValue(QString("Settings/") + key, value);
}

QStringList CFG::get_keys(const char *action) const {
	return settings.value(QString("Keys/") + action, keys[action]).toStringList();
}

