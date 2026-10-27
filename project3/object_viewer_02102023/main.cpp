#include "main_window.h"

#include <QApplication>

static void QtMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(object_viewer);

    qInstallMessageHandler(QtMessageOutput);
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
