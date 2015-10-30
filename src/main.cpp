#include <QApplication>
#include <QString>
#include <QProcess>
#include <iostream>
#include <getopt.h>
#include "download.h"
#include "resourcemanager.h"
#include "viewer.h"
#include "config.h"
#include "dbus/dbus.h"

using namespace std;


static void print_help(char *name) {
	cout << "Usage:" << endl;
	cout << "  " << name << " ([OPTIONS] FILE|(-u URL))*" << endl;
	cout << endl;
	cout << "Settings:" << endl;
	cout << "  -u, --url                         Open a URL instead of a local file" << endl;
	cout << "  -p, --page NUM                    Start showing page NUM" << endl;
	cout << "  -f, --fullscreen                  Start in fullscreen mode" << endl;
	cout << "  -q, --quit true|false             Quit on initialization failure" << endl;
	cout << "  -s, --single-instance true|false  Whether to have a single instance per file" << endl;
	cout << "  --write-default-config FILE       Write the default configuration to FILE and exit" << endl;
	cout << "  -h, --help                        Print this help and exit" << endl;
}

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	// parse command line options
	struct option long_options[] = {
		{"url",						no_argument,		NULL,	'u'},
		{"page",					required_argument,	NULL,	'p'},
		{"fullscreen",				no_argument,		NULL,	'f'},
		{"quit",					required_argument,	NULL,	'q'},
		{"single-instance",			required_argument,	NULL,	's'},
		{"write-default-config",	required_argument,	NULL,	0},
		{"help",					no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0}
	};
	int option_index = 0;
	bool download_url = false;
	while (1) {
		int c = getopt_long(argc, argv, "+up:fq:hs:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
			case 0: {
				const char *option_name = long_options[option_index].name;
				if (!strcmp(option_name, "write-default-config")) {
					CFG::write_defaults(optarg);
					return 0;
				}
				break;
			}
			case 'u':
				download_url = true;
				break;
			case 'p':
				// currently no warning message on wrong input
				CFG::get_instance()->set_tmp_value("start_page", atoi(optarg) - 1);
				break;
			case 'f':
				CFG::get_instance()->set_tmp_value("fullscreen", true);
				break;
			case 'q':
				CFG::get_instance()->set_tmp_value("Settings/quit_on_init_fail", optarg);
				break;
			case 'h':
				print_help(argv[0]);
				return 0;
			case 's':
				// (according to QVariant) any string can be converted to
				// bool, so no type check needed here
				CFG::get_instance()->set_tmp_value("Settings/single_instance_per_file", optarg);
				break;
			case 'c':
				CFG::write_defaults(optarg);
				return 0;
			default:
				// getopt prints an error message
				return 1;
		}
	}

	// fork more processes if there are arguments left
	if (optind < argc - 1) {
		QStringList l;
		for (int i = optind + 1; i < argc; i++) {
			l << argv[i];
		}
		QProcess::startDetached(argv[0], l);
	}

	QString file;
	Download download;
	if (argv[optind] != NULL) {
		if (download_url) {
			file = download.load(QString::fromUtf8(argv[optind]));
		} else {
			file = QString::fromUtf8(argv[optind]);
		}
		if (file.isNull()) {
			return 1;
		}
	} else if (CFG::get_instance()->get_most_current_value("Settings/quit_on_init_fail").toBool()) {
		print_help(argv[0]);
		return 1;
	}
	// else: opens empty window without file

	if (CFG::get_instance()->get_most_current_value("Settings/single_instance_per_file").toBool()) {
		if (activate_katarakt_with_file(file)) {
			return 0;
		}
	}

	// load stylesheet from config if no stylesheet was specified on the command line
	if (app.styleSheet().isEmpty()) {
		app.setStyleSheet(CFG::get_instance()->get_value("Settings/stylesheet").toString());
	}

	Viewer katarakt(file);
	if (!katarakt.is_valid()) {
		return 1;
	}
	katarakt.show();

	// initialize dbus interfaces
	dbus_init(&katarakt);

	return app.exec();
}

