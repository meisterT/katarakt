#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QVector>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QStringList>


class CFG {
private:
	CFG();
	CFG(const CFG &other);
	CFG(const char *file);
	CFG &operator=(const CFG &other);
	~CFG();

	void init_defaults();
	void set_defaults();

	QSettings settings;
	QVector<QString> vd;
	QVector<QString> vk;
	QHash<QString,QVariant> defaults;
	QHash<QString,QVariant> tmp_values; // not persistent
	QHash<QString,QStringList> keys;

public:
	static CFG *get_instance();
	static void write_defaults(const char *file); // write defaults to file

	QVariant get_value(const char *key) const;
	void set_value(const char *key, QVariant value);

	QVariant get_tmp_value(const char *key) const;
	void set_tmp_value(const char *key, QVariant value);
	bool has_tmp_value(const char *key) const;

	/** Return tmp_value (if set) but fall back to normal settings */
	QVariant get_most_current_value(const char *key) const;

	QStringList get_keys(const char *action) const;
};

#endif

