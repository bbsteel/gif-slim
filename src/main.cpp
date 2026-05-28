#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include "mainwindow.h"
#include "cli.h"

static QFile g_logFile;

void logHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    if (!g_logFile.isOpen()) return;
    QTextStream ts(&g_logFile);
    QString level = "INFO";
    if (type == QtWarningMsg)  level = "WARN";
    if (type == QtCriticalMsg) level = "CRIT";
    if (type == QtFatalMsg)    level = "FATAL";
    ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz")
       << " [" << level << "] " << msg << "\n";
    ts.flush();
}

int main(int argc, char *argv[]) {
    g_logFile.setFileName("gif-editor.log");
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    qInstallMessageHandler(logHandler);
    qInfo() << "=== GIF Editor started ===";

    auto cli = parseCli(argc, argv);
    bool headless = cli.headless;

    QApplication app(argc, argv);
    app.setApplicationName("GIF Editor");

    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window,          QColor(30, 30, 30));
    p.setColor(QPalette::WindowText,      QColor(208, 208, 208));
    p.setColor(QPalette::Base,            QColor(42, 42, 42));
    p.setColor(QPalette::AlternateBase,   QColor(50, 50, 50));
    p.setColor(QPalette::Text,            QColor(208, 208, 208));
    p.setColor(QPalette::Button,          QColor(45, 45, 45));
    p.setColor(QPalette::ButtonText,      QColor(208, 208, 208));
    p.setColor(QPalette::BrightText,      QColor(255, 100, 50));
    p.setColor(QPalette::Link,            QColor(100, 160, 255));
    p.setColor(QPalette::Highlight,       QColor(100, 160, 255));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::ToolTipBase,     QColor(50, 50, 50));
    p.setColor(QPalette::ToolTipText,     QColor(208, 208, 208));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(100, 100, 100));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
    app.setPalette(p);

    MainWindow win;

    if (!headless) {
        win.show();
        if (cli.actions.size() == 1 && cli.actions[0].type == CliAction::Open)
            win.openFile(cli.actions[0].arg);
    }

    // 执行 CLI 操作
    for (const auto &act : cli.actions) {
        switch (act.type) {
        case CliAction::Open:
            qInfo() << "CLI open:" << act.arg;
            win.openFile(act.arg);
            break;
        case CliAction::Crop: {
            auto dash = act.arg.indexOf('-');
            int s = act.arg.left(dash).toInt();
            int e = act.arg.mid(dash + 1).toInt();
            qInfo() << "CLI crop:" << s << "-" << e;
            win.setRange(s, e);
            win.cropToRange();
            break;
        }
        case CliAction::Delete: {
            auto dash = act.arg.indexOf('-');
            int s = act.arg.left(dash).toInt();
            int e = act.arg.mid(dash + 1).toInt();
            qInfo() << "CLI delete:" << s << "-" << e;
            win.setRange(s, e);
            win.deleteRange();
            break;
        }
        case CliAction::Skip:
            qInfo() << "CLI skip:" << act.arg;
            win.applySkip(act.arg.toInt());
            break;
        case CliAction::Speed:
            qInfo() << "CLI speed:" << act.arg;
            win.applySpeed(act.arg.toDouble());
            break;
        case CliAction::Scale:
            qInfo() << "CLI scale:" << act.arg;
            win.applyScale(act.arg.toDouble());
            break;
        case CliAction::Save:
            qInfo() << "CLI save:" << act.arg;
            win.saveAs(act.arg);
            break;
        case CliAction::Quit:
            return 0;
        }
    }

    if (headless && !cli.actions.empty()) {
        // 等待事件处理完成，然后退出
        QTimer::singleShot(500, &app, &QApplication::quit);
        return app.exec();
    }

    if (headless) {
        qInfo() << "headless mode with no actions, exiting";
        return 0;
    }

    return app.exec();
}
