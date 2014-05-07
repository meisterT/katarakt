#include "config.h"
#include <QHashIterator>


CFG::CFG() :
		settings(QSettings::IniFormat, QSettings::UserScope, "katarakt") {
	// TODO warn about invalid user input

	settings.beginGroup("Settings");
	// canvas options
	defaults["background_opacity"] = 223;
	defaults["default_layout"] = "presentation";
	defaults["mouse_wheel_factor"] = 120; // (qt-)delta for turning the mouse wheel 1 click
	defaults["smooth_scroll_delta"] = 30; // pixel scroll offset
	defaults["screen_scroll_factor"] = 0.9; // creates overlap for scrolling 1 screen down, should be <= 1
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
	// viewer options
	defaults["quit_on_init_fail"] = false;
	defaults["icon_theme"] = "";
	settings.endGroup();

	tmp_values["start_page"] = 0;
	tmp_values["fullscreen"] = false;

	settings.beginGroup("keys");
	// canvas keys
	keys["set_presentation_layout"] = QStringList() << "1";
	keys["set_grid_layout"] = QStringList() << "2";
	keys["page_up"] = QStringList() << "PgUp";
	keys["page_down"] = QStringList() << "PgDown";
	keys["page_first"] = QStringList() << "Home" << "G";
	keys["page_last"] = QStringList() << "End" << "Shift+G";
	keys["half_screen_up"] = QStringList() << "Ctrl+U";
	keys["half_screen_down"] = QStringList() << "Ctrl+D";
	keys["screen_up"] = QStringList() << "Backspace" << "Ctrl+B";
	keys["screen_down"] = QStringList() << "Space" << "Ctrl+F";
	keys["smooth_up"] = QStringList() << "Up" << "K";
	keys["smooth_down"] = QStringList() << "Down" << "J";
	keys["smooth_left"] = QStringList() << "Left" << "H";
	keys["smooth_right"] = QStringList() << "Right" << "L";
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
	keys["next_invisible_hit"] = QStringList() << "Ctrl+N";
	keys["previous_invisible_hit"] = QStringList() << "Ctrl+Shift+N";
	keys["focus_goto"] = QStringList() << "Ctrl+G";
	keys["rotate_left"] = QStringList() << "U";
	keys["rotate_right"] = QStringList() << "I";
	// viewer keys
	keys["toggle_fullscreen"] = QStringList() << "F";
	keys["close_search"] = QStringList() << "Esc";
	keys["reload"] = QStringList() << "R";
	keys["open"] = QStringList() << "O";
	settings.endGroup();
}

CFG::CFG(const CFG &/*other*/) {
}

CFG &CFG::operator=(const CFG &/*other*/) {
	return *this;
}

CFG::~CFG() {
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

QVariant CFG::get_tmp_value(const char *key) const {
	return tmp_values[key];
}

void CFG::set_tmp_value(const char *key, QVariant value) {
	tmp_values[key] = value;
}

bool CFG::has_tmp_value(const char *key) const {
	return tmp_values.contains(key);
}

QStringList CFG::get_keys(const char *action) const {
	return settings.value(QString("Keys/") + action, keys[action]).toStringList();
}

