#include "source_correlate.h"

#include "../viewer.h"
#include "../canvas.h"
#include "../layout/layout.h"
#include "../resourcemanager.h"

#include <QUrl>
#include <QFileInfo>

using namespace std;

SourceCorrelate::SourceCorrelate(Viewer *viewer)
	: QDBusAbstractAdaptor(viewer), viewer(viewer)
{
	connect(viewer->get_canvas(), SIGNAL(synchronize_editor(int, int, int)),
	        this, SLOT(emit_edit_signal(int, int, int)));
}	

void SourceCorrelate::view(QString filename, int page, int x, int y)
{
	if (page < 0)
		return;
	
	QFileInfo oldFile = QFileInfo(viewer->get_res()->get_file());
	QFileInfo newFile = QFileInfo(filename);
	QUrl url(filename);
	if ((url.isLocalFile() || url.isRelative())
	    && newFile.canonicalFilePath() != oldFile.canonicalFilePath()) {
		// only open, if file is different from the already loaded one
		viewer->open(filename);
	}

	// needed to add an entry to the jump list
	int current_page = viewer->get_canvas()->get_layout()->get_page();

	viewer->get_canvas()->get_layout()->scroll_page(page, false);
	// FIXME scroll to offset
	viewer->get_res()->store_jump(current_page);
	viewer->get_canvas()->repaint();
	viewer->repaint();
}

void SourceCorrelate::emit_edit_signal(int page, int x, int y)
{
	QString file = viewer->get_res()->get_file();
	qDebug("Emitting the edit signal");
	emit edit(file, page, x, y);
}
