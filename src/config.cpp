#include "config.h"
#include <QVectorIterator>
#include <iostream>


using namespace std;


CFG::CFG() :
		settings(QSettings::IniFormat, QSettings::UserScope, "katarakt") {
	// TODO warn about invalid user input
	init_defaults();
}

CFG::CFG(const char* file) :
		settings(file, QSettings::IniFormat) {
	init_defaults();
}

void CFG::init_defaults() {
	// settings
	// canvas
	vd.push_back("Settings/default_layout"); defaults[vd.back()] = "single";
	vd.push_back("Settings/background_color"); defaults[vd.back()] = "0xDF202020";
	vd.push_back("Settings/background_color_fullscreen"); defaults[vd.back()] = "0xFF000000";
	vd.push_back("Settings/unrendered_page_color"); defaults[vd.back()] = "0x40FFFFFF";
	vd.push_back("Settings/click_link_button"); defaults[vd.back()] = 1;
	vd.push_back("Settings/drag_view_button"); defaults[vd.back()] = 2;
	vd.push_back("Settings/select_text_button"); defaults[vd.back()] = 1;
	vd.push_back("Settings/smooth_scroll_delta"); defaults[vd.back()] = 30; // pixel scroll offset
	vd.push_back("Settings/screen_scroll_factor"); defaults[vd.back()] = 0.9; // creates overlap for scrolling 1 screen down, should be <= 1
	vd.push_back("Settings/jump_padding"); defaults[vd.back()] = 0.2; // must be <= 0.5
	vd.push_back("Settings/rect_margin"); defaults[vd.back()] = 2;
	vd.push_back("Settings/useless_gap"); defaults[vd.back()] = 2;
	vd.push_back("Settings/min_zoom"); defaults[vd.back()] = -14;
	vd.push_back("Settings/max_zoom"); defaults[vd.back()] = 30;
	vd.push_back("Settings/zoom_factor"); defaults[vd.back()] = 0.05;
	vd.push_back("Settings/min_page_width"); defaults[vd.back()] = 50;
	vd.push_back("Settings/presenter_slide_ratio"); defaults[vd.back()] = 0.67;
	// viewer
	vd.push_back("Settings/quit_on_init_fail"); defaults[vd.back()] = false;
	vd.push_back("Settings/single_instance_per_file"); defaults[vd.back()] = false;
	vd.push_back("Settings/stylesheet"); defaults[vd.back()] = "";
	vd.push_back("Settings/page_overlay_text"); defaults[vd.back()] = "Page %1/%2";
	vd.push_back("Settings/icon_theme"); defaults[vd.back()] = "";
	// internal
	vd.push_back("Settings/prefetch_count"); defaults[vd.back()] = 4;
	vd.push_back("Settings/mouse_wheel_factor"); defaults[vd.back()] = 120; // (qt-)delta for turning the mouse wheel 1 click
	vd.push_back("Settings/thumbnail_filter"); defaults[vd.back()] = true; // filter when creating thumbnail image
	vd.push_back("Settings/thumbnail_size"); defaults[vd.back()] = 32;

	// keys
	// movement
	vk.push_back("Keys/page_up"); keys[vk.back()] = QStringList() << "PgUp";
	vk.push_back("Keys/page_down"); keys[vk.back()] = QStringList() << "PgDown";
	vk.push_back("Keys/top"); keys[vk.back()] = QStringList() << "Home" << "G";
	vk.push_back("Keys/bottom"); keys[vk.back()] = QStringList() << "End" << "Shift+G";
	vk.push_back("Keys/goto_page"); keys[vk.back()] = QStringList() << "Ctrl+G";
	vk.push_back("Keys/half_screen_up"); keys[vk.back()] = QStringList() << "Ctrl+U";
	vk.push_back("Keys/half_screen_down"); keys[vk.back()] = QStringList() << "Ctrl+D";
	vk.push_back("Keys/screen_up"); keys[vk.back()] = QStringList() << "Backspace" << "Ctrl+B";
	vk.push_back("Keys/screen_down"); keys[vk.back()] = QStringList() << "Space" << "Ctrl+F";
	vk.push_back("Keys/smooth_up"); keys[vk.back()] = QStringList() << "Up" << "K";
	vk.push_back("Keys/smooth_down"); keys[vk.back()] = QStringList() << "Down" << "J";
	vk.push_back("Keys/smooth_left"); keys[vk.back()] = QStringList() << "Left" << "H";
	vk.push_back("Keys/smooth_right"); keys[vk.back()] = QStringList() << "Right" << "L";
	vk.push_back("Keys/search"); keys[vk.back()] = QStringList() << "/";
	vk.push_back("Keys/search_backward"); keys[vk.back()] = QStringList() << "?";
	vk.push_back("Keys/next_hit"); keys[vk.back()] = QStringList() << "N";
	vk.push_back("Keys/previous_hit"); keys[vk.back()] = QStringList() << "Shift+N";
	vk.push_back("Keys/next_invisible_hit"); keys[vk.back()] = QStringList() << "Ctrl+N";
	vk.push_back("Keys/previous_invisible_hit"); keys[vk.back()] = QStringList() << "Ctrl+Shift+N";
	vk.push_back("Keys/jump_back"); keys[vk.back()] = QStringList() << "Ctrl+O" << "Alt+Left";
	vk.push_back("Keys/jump_forward"); keys[vk.back()] = QStringList() << "Ctrl+I" << "Alt+Right";
	vk.push_back("Keys/mark_jump"); keys[vk.back()] = QStringList() << "M";
	// layout
	vk.push_back("Keys/set_single_layout"); keys[vk.back()] = QStringList() << "1";
	vk.push_back("Keys/set_grid_layout"); keys[vk.back()] = QStringList() << "2";
	vk.push_back("Keys/set_presenter_layout"); keys[vk.back()] = QStringList() << "3";
	vk.push_back("Keys/zoom_in"); keys[vk.back()] = QStringList() << "=" << "+";
	vk.push_back("Keys/zoom_out"); keys[vk.back()] = QStringList() << "-";
	vk.push_back("Keys/reset_zoom"); keys[vk.back()] = QStringList() << "Z";
	vk.push_back("Keys/increase_columns"); keys[vk.back()] = QStringList() << "]";
	vk.push_back("Keys/decrease_columns"); keys[vk.back()] = QStringList() << "[";
	vk.push_back("Keys/increase_offset"); keys[vk.back()] = QStringList() << "}";
	vk.push_back("Keys/decrease_offset"); keys[vk.back()] = QStringList() << "{";
	vk.push_back("Keys/rotate_left"); keys[vk.back()] = QStringList() << ",";
	vk.push_back("Keys/rotate_right"); keys[vk.back()] = QStringList() << ".";
	// viewer
	vk.push_back("Keys/toggle_overlay"); keys[vk.back()] = QStringList() << "T";
	vk.push_back("Keys/quit"); keys[vk.back()] = QStringList() << "Q" << "W,E,E,E";
	vk.push_back("Keys/close_search"); keys[vk.back()] = QStringList() << "Esc";
	vk.push_back("Keys/invert_colors"); keys[vk.back()] = QStringList() << "I";
	vk.push_back("Keys/copy_to_clipboard"); keys[vk.back()] = QStringList() << "Ctrl+C";
	vk.push_back("Keys/swap_selection_and_panning_buttons"); keys[vk.back()] = QStringList() << "V";
	vk.push_back("Keys/toggle_fullscreen"); keys[vk.back()] = QStringList() << "F";
	vk.push_back("Keys/reload"); keys[vk.back()] = QStringList() << "R";
	vk.push_back("Keys/open"); keys[vk.back()] = QStringList() << "O";
	vk.push_back("Keys/print"); keys[vk.back()] = QStringList() << "P";
	vk.push_back("Keys/save"); keys[vk.back()] = QStringList() << "S";
	vk.push_back("Keys/toggle_toc"); keys[vk.back()] = QStringList() << "F9";

	// tmp values
	tmp_values["start_page"] = 0;
	tmp_values["fullscreen"] = false;
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
	QVectorIterator<QString> it(vd);
	while (it.hasNext()) {
		QString tmp = it.next();
		settings.setValue(tmp, defaults[tmp]);
	}

	QVectorIterator<QString> i2(vk);
	while (i2.hasNext()) {
		QString tmp = i2.next();
		settings.setValue(tmp, keys[tmp]);
	}
}

CFG *CFG::get_instance() {
	static CFG instance;
	return &instance;
}

void CFG::write_defaults(const char *file) {
	CFG inst(file);
	inst.settings.clear();
	inst.settings.setFallbacksEnabled(false);
	inst.set_defaults();
	inst.settings.sync();
	get_instance();
}

QVariant CFG::get_value(const char *key) const {
#ifdef DEBUG
	if (defaults.find(key) == defaults.end()) {
		cout << "missing key " << key << endl;
	}
#endif
	return settings.value(key, defaults[key]);
}

void CFG::set_value(const char *key, QVariant value) {
	settings.setValue(key, value);
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

QVariant CFG::get_most_current_value(const char *key) const {
	if (tmp_values.contains(key)) {
		return tmp_values[key];
	} else {
#ifdef DEBUG
		if (defaults.find(key) == defaults.end()) {
			cout << "missing key " << key << endl;
		}
#endif
		return settings.value(key, defaults[key]);
	}
}

QStringList CFG::get_keys(const char *action) const {
	return settings.value(action, keys[action]).toStringList();
}

