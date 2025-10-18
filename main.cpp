#include "dialog.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // that supports both old GL (for drawing) and new features (for texture).
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    Dialog w;
    w.show();
    return a.exec();
}
