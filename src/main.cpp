#include <QCoreApplication>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include "app/designer.hpp"
#include "app/meadbridge.hpp"
#include "app/theme.hpp"

namespace {
// If the MEAD_LOG environment variable names a file (the local build script sets it), screen (QML) warnings and errors
// go there, so problems can be read even though the app has no console window.
QFile *g_log = nullptr;
QtMessageHandler g_previous = nullptr;

void logMessage(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    static QMutex mutex;
    {
        QMutexLocker lock(&mutex);
        if (g_log && g_log->isOpen()) {
            static const char *kinds[] = {"debug", "warning", "critical", "fatal", "info"};
            const int k = static_cast<int>(type);
            QTextStream out(g_log);
            out << (k >= 0 && k < 5 ? kinds[k] : "msg") << ": " << msg;
            if (ctx.file) out << "  (" << ctx.file << ":" << ctx.line << ")";
            out << "\n";
            out.flush();
        }
    }
    if (g_previous) g_previous(type, ctx, msg);
}
}  // namespace

int main(int argc, char *argv[])
{
    const QString logPath = qEnvironmentVariable("MEAD_LOG");
    if (!logPath.isEmpty()) {
        g_log = new QFile(logPath);
        if (g_log->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            g_previous = qInstallMessageHandler(logMessage);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/packaging/icons/meadworkshop.png")));
    QGuiApplication::setDesktopFileName(QStringLiteral("meadworkshop"));   // Linux: matches meadworkshop.desktop
    QGuiApplication::setOrganizationName(QStringLiteral("MeadWorkshop"));
    QGuiApplication::setApplicationName(QStringLiteral("Mead Workshop"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(MEAD_VERSION));   // from project() in CMakeLists.txt

    // "Basic" is Qt's plain, fully themeable control style — we apply our own dark/light colours.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // The bridge between the screens (QML) and the brewing math (C++).
    MeadBridge bridge;
    qmlRegisterSingletonInstance("MeadWorkshop.Core", 1, 0, "Mead", &bridge);
    // The recipe designer: holds the recipe being designed and everything the Designer screen shows.
    Designer designer;
    qmlRegisterSingletonInstance("MeadWorkshop.Core", 1, 0, "Designer", &designer);
    // Colours for every screen (dark/light).
    Theme theme;
    qmlRegisterSingletonInstance("MeadWorkshop.Core", 1, 0, "Theme", &theme);

    QQmlApplicationEngine engine;
    // The installer puts Qt's screen components in a "qml" folder next to MeadWorkshop.exe; MSYS2's Qt would
    // otherwise look only in its own install folder and the app would close before showing a window.
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("MeadWorkshop", "Main");

    return app.exec();
}
