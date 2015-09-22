#ifndef DBUS_H
#define DBUS_H

#include <QString>
class Viewer;

/**
 * Connect to the session bus and publish all interfaces.
 *
 * Fails silently in case of errors.
 */
void dbus_init(Viewer *viewer);

/**
 * Try to find a katarakt instance that has file open
 * If such a instance is found, it is activaed using the focus() method via
 * dbus and true is returned (false otherwise).
 *
 * This method works without dbus_init() being called before!
 */
bool activate_katarakt_with_file(QString file);

#endif /* DBUS_H */
