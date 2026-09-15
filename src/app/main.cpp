#include "app_controller.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPainter>
#include <qqml.h>

#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IssueTrace"));
    QApplication::setOrganizationName(QStringLiteral("IssueTrace"));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/issuetrace/resources/icons/issuetrace-256.png")));

    AppController controller;
    qmlRegisterSingletonInstance("IssueTrace", 1, 0, "App", &controller);
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("IssueTrace", "Main");
    std::unique_ptr<QObject> rootObject(component.create(engine.rootContext()));
    if (!rootObject) {
        QTextStream errorStream(stderr);
        errorStream << "IssueTrace failed to load its interface:\n";
        for (const auto& error : component.errors()) {
            errorStream << error.toString() << '\n';
        }
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(rootObject.get());
    if (!window) return 1;
    window->show();
    std::unique_ptr<QMenu> trayMenu;
    std::unique_ptr<QSystemTrayIcon> tray;
    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    QApplication::setQuitOnLastWindowClosed(!trayAvailable);
    if (trayAvailable) {
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(QStringLiteral("#3b82f6")));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(4, 4, 56, 56), 14, 14);
        painter.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(18, 23), QPointF(46, 23));
        painter.drawLine(QPointF(18, 34), QPointF(40, 34));
        painter.drawLine(QPointF(18, 45), QPointF(34, 45));
        painter.end();
        tray = std::make_unique<QSystemTrayIcon>(QIcon(pixmap));
        trayMenu = std::make_unique<QMenu>();
        auto* menu = trayMenu.get();
        menu->addAction(QStringLiteral("显示 IssueTrace"), window, [window] {
            window->show();
            window->raise();
            window->requestActivate();
        });
        menu->addSeparator();
        menu->addAction(QStringLiteral("退出"), &app, &QCoreApplication::quit);
        tray->setContextMenu(menu);
        tray->setToolTip(QStringLiteral("IssueTrace 问题提醒"));
        QObject::connect(tray.get(), &QSystemTrayIcon::activated, window,
            [window](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    window->show();
                    window->raise();
                    window->requestActivate();
                }
            });
        QObject::connect(&controller, &AppController::reminderDue, tray.get(),
            [icon = tray.get(), window](const QString&, const QString& title,
                                        const QString& message) {
                icon->showMessage(title, message, QSystemTrayIcon::Information, 15000);
                if (!window->isVisible()) icon->setToolTip(title + QStringLiteral(" · 待处理"));
            });
        tray->show();
    }
    QTimer reminderTimer;
    reminderTimer.setInterval(30000);
    QObject::connect(&reminderTimer, &QTimer::timeout,
                     &controller, &AppController::checkReminders);
    reminderTimer.start();
    QTimer::singleShot(0, &controller, &AppController::checkReminders);
    const auto arguments = QCoreApplication::arguments();
    if (arguments.contains(QStringLiteral("--verify-scroll-layout"))) {
        controller.createQuickIssue(QStringLiteral("界面滚动范围验证"), QString{});
        for (int index = 1; index <= 30; ++index) {
            controller.addTimelineEntry(
                QStringLiteral("note"),
                QStringLiteral("滚动验证记录 %1：用于确认较长的问题处理过程仍可上下滚动。")
                    .arg(index));
        }
        QTimer::singleShot(100, &app, [&app, window] {
            const auto* scroll = window->findChild<QObject*>(
                QStringLiteral("detailScroll"));
            const auto contentHeight = scroll
                ? scroll->property("contentHeight").toReal() : 0.0;
            const auto viewportHeight = scroll
                ? scroll->property("height").toReal() : 0.0;
            if (!scroll || contentHeight <= viewportHeight + 1.0) {
                QTextStream(stderr)
                    << "IssueTrace scroll layout check failed: contentHeight="
                    << contentHeight << ", viewportHeight=" << viewportHeight << '\n';
                app.exit(3);
                return;
            }
            app.exit(0);
        });
    }
    if (arguments.contains(QStringLiteral("--verify-status-refresh"))) {
        controller.createQuickIssue(QStringLiteral("状态刷新验证"), QString{});
        QTimer::singleShot(100, &app, [&app, window, &controller] {
            auto* investigating = window->findChild<QObject*>(
                QStringLiteral("statusInvestigatingButton"));
            const auto invoked = investigating &&
                QMetaObject::invokeMethod(investigating, "clicked");
            QTimer::singleShot(150, &app, [&app, window, &controller, invoked] {
                auto* pending = window->findChild<QObject*>(
                    QStringLiteral("statusPendingButton"));
                auto* active = window->findChild<QObject*>(
                    QStringLiteral("statusInvestigatingButton"));
                auto* inbox = window->findChild<QObject*>(QStringLiteral("issueInbox"));
                const auto selectedStatus = controller.selectedIssueStatus();
                QString listedStatus;
                const auto selectedId = controller.selectedIssue()
                                            .value(QStringLiteral("id")).toString();
                for (const auto& item : controller.issues()) {
                    const auto issue = item.toMap();
                    if (issue.value(QStringLiteral("id")).toString() == selectedId) {
                        listedStatus = issue.value(QStringLiteral("status")).toString();
                        break;
                    }
                }
                if (!invoked || !pending || !active || !inbox ||
                    pending->property("highlighted").toBool() ||
                    !active->property("highlighted").toBool() ||
                    inbox->property("selectedIssueStatusText").toString() !=
                        QStringLiteral("处理中") ||
                    selectedStatus != QStringLiteral("investigating") ||
                    listedStatus != QStringLiteral("investigating")) {
                    QTextStream(stderr)
                        << "IssueTrace status refresh check failed: invoked=" << invoked
                        << ", selected=" << selectedStatus
                        << ", listed=" << listedStatus
                        << ", pending="
                        << (pending ? pending->property("highlighted").toBool() : false)
                        << ", investigating="
                        << (active ? active->property("highlighted").toBool() : false)
                        << ", inbox="
                        << (inbox ? inbox->property("selectedIssueStatusText").toString()
                                  : QString{})
                        << '\n';
                    app.exit(4);
                    return;
                }
                app.exit(0);
            });
        });
    }
    if (arguments.contains(QStringLiteral("--verify-custom-reminder"))) {
        controller.createQuickIssue(QStringLiteral("自定义提醒验证"), QString{});
        const auto reminder = QDateTime::currentDateTime().addDays(2)
                                  .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const auto saved = controller.remindSelectedIssueAt(reminder);
        const auto projected = controller.selectedIssue()
                                   .value(QStringLiteral("remindAt")).toString();
        const auto invalidAccepted = controller.remindSelectedIssueAt(
            QStringLiteral("不是时间"));
        if (!saved || invalidAccepted || projected != reminder) {
            QTextStream(stderr)
                << "IssueTrace custom reminder check failed: saved=" << saved
                << ", invalidAccepted=" << invalidAccepted
                << ", expected=" << reminder << ", projected=" << projected << '\n';
            return 5;
        }
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }
    const auto markerOption = arguments.indexOf(QStringLiteral("--update-health-marker"));
    if (markerOption >= 0 && markerOption + 1 < arguments.size()) {
        const QFileInfo markerInfo(arguments.at(markerOption + 1));
        if (markerInfo.fileName().startsWith(QStringLiteral(".issuetrace-health-")) &&
            markerInfo.dir().exists()) {
            QSaveFile marker(markerInfo.absoluteFilePath());
            if (marker.open(QIODevice::WriteOnly)) {
                marker.write("IssueTrace " ISSUETRACE_APP_VERSION " started\n");
                marker.commit();
            }
        }
    }
    if (qEnvironmentVariableIsSet("ISSUETRACE_EXIT_AFTER_HEALTH")) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
