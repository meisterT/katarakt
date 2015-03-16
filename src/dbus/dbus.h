#ifndef DBUS_H
#define DBUS_H

class Viewer;

/**
 * Connect to the session bus and publish all interfaces.
 *
 * Fails silently in case of errors.
 */
void dbus_init(Viewer *viewer);

#endif /* DBUS_H */
