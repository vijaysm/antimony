#include <Python.h>

#include <QDebug>

#include <QCommandLineParser>
#include <QMainWindow>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QTextCodec>
#include <QStringList>
#include <QMessageBox>

#include "app/app.h"
#include "graph/hooks/hooks.h"

#include "fab/fab.h"
#include "graph/graph.h"

int main(int argc, char *argv[])
{
    // Use UTF-8, ignoring any LANG settings in the environment
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    {   // Set the default OpenGL version to be 2.1 with sample buffers
        QSurfaceFormat format;
        format.setVersion(2, 1);
        QSurfaceFormat::setDefaultFormat(format);
    }

    // Create the Application object
    App app(argc, argv);

#if defined Q_OS_MAC
    if (QCoreApplication::applicationDirPath().startsWith("/Volumes/"))
    {
        QMessageBox::critical(NULL, "Cannot run from disk image",
                "Antimony cannot run from a disk image.\n"
                "Please copy it out of the disk image and relaunch.");
        exit(1);
    }
#endif

    // Initialize various Python modules and the interpreter itself
    fab::preInit();
    Graph::preInit();
    AppHooks::preInit();
    Py_Initialize();

    // Set locale to C to make atof correctly parse floats
    setlocale(LC_NUMERIC, "C");

    {   // Modify Python's default search path to include the application's
        // directory (as this doesn't happen on Linux by default)
        QString d = QCoreApplication::applicationDirPath();
#if defined Q_OS_MAC
        QStringList path = d.split("/");
        for (int i=0; i < 3; ++i)
            path.removeLast();
        d = path.join("/");
#endif
        d += "/sb";
        fab::postInit(d.toStdString().c_str());
    }

    {   // Install operator.or_ as a reducer for shapes
        auto op = PyImport_ImportModule("operator");
        Datum::installReducer(fab::ShapeType, PyObject_GetAttrString(op, "or_"));
        Py_DECREF(op);
    }

    {   // Check to make sure that the fab module exists
        PyObject* fab = PyImport_ImportModule("fab");
        if (!fab)
        {
            PyErr_Print();
            QMessageBox::critical(NULL, "Import error",
                    "Import Error:<br><br>"
                    "Could not find <tt>fab</tt> Python module.<br>"
                    "Antimony will now exit.");
            exit(1);
        }
        Py_DECREF(fab);
    }

    {   // Parse command-line arguments
        QCommandLineParser parser;
        parser.setApplicationDescription("CAD from a parallel universe");
        parser.addHelpOption();
        QCommandLineOption forceHeightmap("heightmap",
                "Open 3D windows in heightmap mode");
        parser.addOption(forceHeightmap);
        parser.addPositionalArgument("file", "File to open", "[file]");

        parser.process(app);

        auto args = parser.positionalArguments();
        if (args.length() > 1)
        {
            qCritical("Too many command-line arguments");
            exit(1);
        }
        else if (args.length() == 1)
        {
            app.loadFile(args[0]);
        }
    }

    app.makeDefaultWindows();
    return app.exec();
}
