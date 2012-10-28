#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QStringList>


class CFG {
private:
	CFG();
	CFG(const CFG &other);
	CFG &operator=(const CFG &other);
	~CFG();

	void set_defaults();

	QSettings settings;
	QHash<QString,QVariant> defaults;
	QHash<QString,QStringList> keys;

public:
	static CFG *get_instance();

	QVariant get_value(const char *key) const;
	void set_value(const char *key, QVariant value);

	QStringList get_keys(const char *action) const;
};

#endif

