#ifndef SOURCE_CORRELATE_H
#define SOURCE_CORRELATE_H

#include <QDBusAbstractAdaptor>

class Viewer;

class SourceCorrelate : public QDBusAbstractAdaptor {
	Q_OBJECT;
	Q_CLASSINFO("D-Bus Interface", "katarakt.SourceCorrelate");

public:
	SourceCorrelate(Viewer *viewer);

public slots: 
	/** Jump to a (0 indexed) page in file filename and make the position
	 * (x,y) in points from the top left corner of page visible.
	 *
	 * The document will not be reloaded if the filename is the same as
	 * the currently loaded file.
	 */
	void view(QString filename, int page, double x, double y);

	/** Lets the katarakt window ask the window manager for the focus
	 */
	void focus();

	/** The full filepath of the opened file
	 */
	QString filepath();

signals:
	/** Emitted, if the user requests to edit the source code for a
	 *  specific position on a specific page.
	 * 
	 * The page is 0-indexed.
	 */
	void edit(QString filename, int page, int x, int y);

private slots:
	void emit_edit_signal(int page, int x, int y);

private:
	Viewer *viewer;
};

#endif /* SOURCE_CORRELATE_H */
