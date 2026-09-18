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
#include <QTemporaryDir>
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
        tray->setToolTip(QStringLiteral("IssueTrace 事件提醒"));
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
                QStringLiteral("滚动验证记录 %1：用于确认较长的事件处理过程仍可上下滚动。")
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
    if (arguments.contains(QStringLiteral("--verify-description-image"))) {
        controller.createQuickIssue(QStringLiteral("事件描述图片验证"), QString{});
        auto* details = window->findChild<QObject*>(QStringLiteral("eventDetailsDialog"));
        const auto opened = details && QMetaObject::invokeMethod(details, "open");
        auto temporary = std::make_shared<QTemporaryDir>();
        const auto imagePath = temporary->filePath(QStringLiteral("描述图片.png"));
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::blue);
        const auto imageSaved = image.save(imagePath);
        QTimer::singleShot(150, &app,
            [&app, window, &controller, opened, imageSaved, imagePath, temporary] {
                const auto* chooseButton = window->findChild<QObject*>(
                    QStringLiteral("descriptionChooseImageButton"));
                const auto added = imageSaved && controller.addDescriptionImage(
                    QUrl::fromLocalFile(imagePath));
                const auto projected = controller.descriptionAttachments();
                if (!opened || !chooseButton || !chooseButton->property("visible").toBool() ||
                    !added || projected.size() != 1) {
                    QTextStream(stderr)
                        << "IssueTrace description image check failed: opened=" << opened
                        << ", button=" << (chooseButton != nullptr)
                        << ", imageSaved=" << imageSaved << ", added=" << added
                        << ", projected=" << projected.size() << '\n';
                    app.exit(6);
                    return;
                }
                app.exit(0);
            });
    }
    if (arguments.contains(QStringLiteral("--verify-tracked-duration-edit"))) {
        controller.createQuickIssue(QStringLiteral("计时修正验证"), QString{});
        const auto saved = controller.setSelectedIssueTrackedDuration(2, 15);
        const auto exactDuration = controller.selectedIssue()
                                       .value(QStringLiteral("trackedMilliseconds"))
                                       .toLongLong() == 8'100'000;
        const auto started = controller.startSelectedIssueTimer();
        const auto runningEditRejected =
            !controller.setSelectedIssueTrackedDuration(3, 0);
        QTimer::singleShot(30, &app,
            [&app, window, &controller, saved, exactDuration, started,
             runningEditRejected] {
                const auto paused = controller.pauseSelectedIssueTimer();
                const auto accumulatedFromEditedBase = controller.selectedIssue()
                    .value(QStringLiteral("trackedMilliseconds")).toLongLong() >
                    8'100'000;
                auto* details = window->findChild<QObject*>(
                    QStringLiteral("eventDetailsDialog"));
                const auto opened = details && QMetaObject::invokeMethod(details, "open");
                QTimer::singleShot(150, &app,
                    [&app, window, saved, exactDuration, started,
                     runningEditRejected, paused, accumulatedFromEditedBase,
                     opened] {
                const auto* heading = window->findChild<QObject*>(
                    QStringLiteral("managementEventHeading"));
                const auto* hours = window->findChild<QObject*>(
                    QStringLiteral("trackedHoursEditor"));
                const auto* minutes = window->findChild<QObject*>(
                    QStringLiteral("trackedMinutesEditor"));
                const auto* saveButton = window->findChild<QObject*>(
                    QStringLiteral("saveTrackedDurationButton"));
                const auto valid = saved && exactDuration && started &&
                    runningEditRejected && paused && accumulatedFromEditedBase &&
                    opened && heading && hours && minutes && saveButton &&
                    heading->property("text").toString() == QStringLiteral("事件") &&
                    hours->property("value").toInt() == 2 &&
                    minutes->property("value").toInt() == 15 &&
                    saveButton->property("enabled").toBool();
                if (!valid) {
                    QTextStream(stderr)
                        << "IssueTrace tracked duration edit check failed: saved="
                        << saved << ", exact=" << exactDuration
                        << ", started=" << started
                        << ", runningRejected=" << runningEditRejected
                        << ", paused=" << paused
                        << ", accumulated=" << accumulatedFromEditedBase
                        << ", opened=" << opened
                        << ", heading=" << (heading != nullptr)
                        << ", hours=" << (hours ? hours->property("value").toInt() : -1)
                        << ", minutes="
                        << (minutes ? minutes->property("value").toInt() : -1)
                        << ", button=" << (saveButton != nullptr) << '\n';
                    app.exit(8);
                    return;
                }
                app.exit(0);
                    });
            });
    }
    if (arguments.contains(QStringLiteral("--verify-explicit-metadata-save"))) {
        controller.createQuickIssue(QStringLiteral("历史人员样本"),
                                    QStringLiteral("张三 111"),
                                    QStringLiteral("李四 222"));
        controller.createQuickIssue(QStringLiteral("待编辑事件"),
                                    QStringLiteral("旧提出人"),
                                    QStringLiteral("旧处理人"));
        const auto targetId = controller.selectedIssue()
                                  .value(QStringLiteral("id")).toString();
        auto* details = window->findChild<QObject*>(
            QStringLiteral("eventDetailsDialog"));
        const auto opened = details && QMetaObject::invokeMethod(details, "open");
        QTimer::singleShot(150, &app,
            [&app, window, &controller, details, opened, targetId] {
                auto* reporter = window->findChild<QObject*>(
                    QStringLiteral("metadata-reporter"));
                auto* assignee = window->findChild<QObject*>(
                    QStringLiteral("metadata-assignee"));
                auto* save = window->findChild<QObject*>(
                    QStringLiteral("saveIssueDetailsButton"));
                const auto reporterIndex = controller.reporterOptions().indexOf(
                    QStringLiteral("张三 111"));
                const auto assigneeIndex = controller.assigneeOptions().indexOf(
                    QStringLiteral("李四 222"));
                const auto selectReporter = reporter && reporterIndex >= 0 &&
                    reporter->setProperty("currentIndex", reporterIndex) &&
                    QMetaObject::invokeMethod(reporter, "activated",
                                              Q_ARG(int, reporterIndex));
                const auto selectAssignee = assignee && assigneeIndex >= 0 &&
                    assignee->setProperty("currentIndex", assigneeIndex) &&
                    QMetaObject::invokeMethod(assignee, "activated",
                                              Q_ARG(int, assigneeIndex));
                const auto editTitle = details && QMetaObject::invokeMethod(
                    details, "setDraftField",
                    Q_ARG(QVariant, QVariant(QStringLiteral("title"))),
                    Q_ARG(QVariant, QVariant(QStringLiteral("  用户输入 保留  "))));
                const auto backendUnchanged =
                    controller.selectedIssue().value(QStringLiteral("reporter")).toString() ==
                        QStringLiteral("旧提出人") &&
                    controller.selectedIssue().value(QStringLiteral("assignee")).toString() ==
                        QStringLiteral("旧处理人");
                controller.selectIssue(targetId);
                const auto draft = details
                    ? details->property("draftIssue").toMap() : QVariantMap{};
                const auto draftPreserved =
                    draft.value(QStringLiteral("reporter")).toString() ==
                        QStringLiteral("张三 111") &&
                    draft.value(QStringLiteral("assignee")).toString() ==
                        QStringLiteral("李四 222") &&
                    draft.value(QStringLiteral("title")).toString() ==
                        QStringLiteral("  用户输入 保留  ");
                const auto saveInvoked = save && save->property("enabled").toBool() &&
                    QMetaObject::invokeMethod(save, "clicked");
                QTimer::singleShot(100, &app,
                    [&app, &controller, opened, reporter, assignee, save,
                     selectReporter, selectAssignee, editTitle, backendUnchanged,
                     draftPreserved, saveInvoked] {
                        const auto saved = controller.selectedIssue();
                        const auto valid = opened && reporter && assignee && save &&
                            selectReporter && selectAssignee && editTitle &&
                            backendUnchanged && draftPreserved && saveInvoked &&
                            saved.value(QStringLiteral("reporter")).toString() ==
                                QStringLiteral("张三 111") &&
                            saved.value(QStringLiteral("assignee")).toString() ==
                                QStringLiteral("李四 222") &&
                            saved.value(QStringLiteral("title")).toString() ==
                                QStringLiteral("  用户输入 保留  ");
                        if (!valid) {
                            QTextStream(stderr)
                                << "IssueTrace explicit metadata save check failed: opened="
                                << opened << ", selectReporter=" << selectReporter
                                << ", selectAssignee=" << selectAssignee
                                << ", editTitle=" << editTitle
                                << ", backendUnchanged=" << backendUnchanged
                                << ", draftPreserved=" << draftPreserved
                                << ", saveInvoked=" << saveInvoked
                                << ", savedReporter="
                                << saved.value(QStringLiteral("reporter")).toString()
                                << ", savedAssignee="
                                << saved.value(QStringLiteral("assignee")).toString()
                                << ", savedTitle='"
                                << saved.value(QStringLiteral("title")).toString()
                                << "'\n";
                            app.exit(9);
                            return;
                        }
                        app.exit(0);
                    });
            });
    }
    if (arguments.contains(QStringLiteral("--verify-two-pane-navigation"))) {
        const auto token = QString::number(QCoreApplication::applicationPid());
        const auto sourceParent = QStringLiteral("支付域-") + token;
        const auto targetGroup = QStringLiteral("目标域-") + token;
        const auto movedParent = targetGroup + QStringLiteral("/") + sourceParent;
        controller.createQuickIssue(QStringLiteral("支付域事件"), QString{});
        auto grouped = controller.selectedIssue();
        grouped.insert(QStringLiteral("group_name"), sourceParent + QStringLiteral("/回调"));
        grouped.insert(QStringLiteral("tags"), QStringLiteral("线上,超时"));
        grouped.insert(QStringLiteral("service"), QStringLiteral("支付服务"));
        grouped.insert(QStringLiteral("version"), QStringLiteral("v2.3"));
        grouped.insert(QStringLiteral("ticket"), QStringLiteral("INC-100"));
        const auto groupedSaved = controller.saveIssue(grouped);
        controller.createQuickIssue(QStringLiteral("目标域事件"), QString{});
        auto target = controller.selectedIssue();
        target.insert(QStringLiteral("group_name"), targetGroup);
        const auto targetSaved = controller.saveIssue(target);
        const auto sortPeer = QStringLiteral("排序参照-") + token;
        controller.createQuickIssue(QStringLiteral("排序参照事件"), QString{});
        auto peer = controller.selectedIssue();
        peer.insert(QStringLiteral("group_name"), sortPeer);
        const auto peerSaved = controller.saveIssue(peer);
        const auto defaultCreated = controller.createQuickIssue(
            QStringLiteral("默认分组事件"), QString{});
        const auto quickMetadataCreated = controller.createQuickIssue(
            QStringLiteral("快捷字段事件"), QStringLiteral("王五"),
            QStringLiteral("赵六"), sourceParent, QStringLiteral("回归,快捷"),
            QStringLiteral("v9.0"), QStringLiteral("快捷服务"),
            QStringLiteral("high"), QStringLiteral("快速记录中的事件描述"),
            QStringLiteral("INC-QUICK"));
        const auto quickFormSaved =
            controller.selectedIssue().value(QStringLiteral("original_problem")).toString() ==
                QStringLiteral("快速记录中的事件描述") &&
            controller.selectedIssue().value(QStringLiteral("ticket")).toString() ==
                QStringLiteral("INC-QUICK");
        const auto groupMoved = controller.moveIssueGroup(
            sourceParent, targetGroup, QStringLiteral("child"));
        const auto groupSortedAfter = controller.moveIssueGroup(
            targetGroup, sortPeer, QStringLiteral("after"));
        bool afterPlacementWorked = false;
        {
            int targetIndex = -1;
            int peerIndex = -1;
            int index = 0;
            for (const auto& value : controller.issueGroups()) {
                const auto path = value.toMap().value(QStringLiteral("path")).toString();
                if (path == targetGroup) targetIndex = index;
                if (path == sortPeer) peerIndex = index;
                ++index;
            }
            afterPlacementWorked = targetIndex > peerIndex && peerIndex >= 0;
        }
        const auto groupSortedBefore = controller.moveIssueGroup(
            targetGroup, sortPeer, QStringLiteral("before"));
        controller.filterIssues(QStringLiteral("支付域事件"), QString{},
                                QStringLiteral("支付服务"), QString{},
                                QStringLiteral("updated_desc"), 0, QString{},
                                QStringLiteral("线上,超时"), QString{}, 0, 0, movedParent,
                                QStringLiteral("2.3"), QStringLiteral("INC-1"));
        QTimer::singleShot(100, &app,
            [&app, window, &controller, groupedSaved, targetSaved, peerSaved,
             defaultCreated, quickMetadataCreated, quickFormSaved, groupMoved, groupSortedAfter,
             groupSortedBefore,
             movedParent, targetGroup, sortPeer, afterPlacementWorked] {
                const auto* editorList = window->findChild<QObject*>(
                    QStringLiteral("issueEditorList"));
                const auto* inbox = window->findChild<QObject*>(
                    QStringLiteral("issueInbox"));
                auto* managementTab = window->findChild<QObject*>(
                    QStringLiteral("managementTab"));
                auto* recordTab = window->findChild<QObject*>(
                    QStringLiteral("recordTab"));
                auto* calendarTab = window->findChild<QObject*>(
                    QStringLiteral("calendarTab"));
                auto* navigationTabs = window->findChild<QObject*>(
                    QStringLiteral("mainNavigationTabs"));
                const auto* calendar = window->findChild<QObject*>(
                    QStringLiteral("issueCalendar"));
                const auto* managementSearch = window->findChild<QObject*>(
                    QStringLiteral("managementSearchInput"));
                auto* advancedFilters = window->findChild<QObject*>(
                    QStringLiteral("advancedFiltersButton"));
                const auto* advancedPanel = window->findChild<QObject*>(
                    QStringLiteral("advancedFiltersPanel"));
                const auto openedAdvancedFilters = advancedFilters &&
                    QMetaObject::invokeMethod(advancedFilters, "activate");
                const auto advancedFilterLayoutReady = openedAdvancedFilters &&
                    managementSearch && advancedPanel &&
                    advancedPanel->property("visible").toBool();
                bool hasParentGroup = false;
                bool hasChildGroup = false;
                bool hasDefaultGroup = false;
                int targetGroupIndex = -1;
                int sortPeerIndex = -1;
                int groupIndex = 0;
                for (const auto& value : controller.issueGroups()) {
                    const auto group = value.toMap();
                    const auto path = group.value(QStringLiteral("path")).toString();
                    hasParentGroup = hasParentGroup || path == movedParent;
                    hasChildGroup = hasChildGroup ||
                        path == movedParent + QStringLiteral("/回调");
                    hasDefaultGroup = hasDefaultGroup ||
                        (path == QStringLiteral("__default__") &&
                         group.value(QStringLiteral("count")).toInt() >= 1);
                    if (path == targetGroup) targetGroupIndex = groupIndex;
                    if (path == sortPeer) sortPeerIndex = groupIndex;
                    ++groupIndex;
                }
                const auto beforePlacementWorked = targetGroupIndex >= 0 &&
                    targetGroupIndex < sortPeerIndex;
                const auto hasMovableDragProxy = inbox &&
                    inbox->property("groupDragUsesMovableProxy").toBool();
                const auto openedRecord = recordTab &&
                    QMetaObject::invokeMethod(recordTab, "activate");
                const auto recordExclusive = openedRecord && managementTab &&
                    !managementTab->property("checked").toBool() &&
                    recordTab->property("checked").toBool() && calendarTab &&
                    !calendarTab->property("checked").toBool();
                const auto openedCalendar = calendarTab &&
                    QMetaObject::invokeMethod(calendarTab, "activate");
                const auto calendarExclusive = openedCalendar && managementTab &&
                    recordTab && !managementTab->property("checked").toBool() &&
                    !recordTab->property("checked").toBool() &&
                    calendarTab->property("checked").toBool() &&
                    navigationTabs->property("currentIndex").toInt() == 2;
                const auto openedManagement = managementTab &&
                    QMetaObject::invokeMethod(managementTab, "activate");
                const auto managementExclusive = openedManagement && navigationTabs &&
                    managementTab->property("checked").toBool() &&
                    !recordTab->property("checked").toBool() &&
                    !calendarTab->property("checked").toBool() &&
                    navigationTabs->property("currentIndex").toInt() == 0;
                const auto historyOptionsReady =
                    controller.reporterOptions().contains(QStringLiteral("王五")) &&
                    controller.assigneeOptions().contains(QStringLiteral("赵六")) &&
                    controller.groupOptions().contains(movedParent) &&
                    controller.versionOptions().contains(QStringLiteral("v9.0")) &&
                    controller.serviceOptions().contains(QStringLiteral("快捷服务"));
                if (!editorList || !groupedSaved || !targetSaved || !peerSaved ||
                    !calendar || !advancedFilterLayoutReady || !defaultCreated ||
                    !quickMetadataCreated || !quickFormSaved ||
                    !groupMoved || !groupSortedAfter ||
                    !groupSortedBefore || !afterPlacementWorked ||
                    controller.editorIssues().size() < 5 ||
                    controller.calendarIssues().size() < 5 ||
                    controller.issues().isEmpty() || !hasParentGroup ||
                    !hasChildGroup || !hasDefaultGroup || !hasMovableDragProxy ||
                    !beforePlacementWorked || !historyOptionsReady || !recordExclusive ||
                    !calendarExclusive || !managementExclusive) {
                    QTextStream(stderr)
                        << "IssueTrace two-pane navigation check failed: editor="
                        << (editorList != nullptr)
                        << ", editorIssues=" << controller.editorIssues().size()
                        << ", groupedIssues=" << controller.issues().size()
                        << ", parent=" << hasParentGroup
                        << ", child=" << hasChildGroup
                        << ", default=" << hasDefaultGroup
                        << ", movableDragProxy=" << hasMovableDragProxy
                        << ", afterPlacement=" << afterPlacementWorked
                        << ", beforePlacement=" << beforePlacementWorked
                        << ", historyOptions=" << historyOptionsReady
                        << ", advancedFilters=" << advancedFilterLayoutReady
                        << ", reporters=" << controller.reporterOptions().join(',')
                        << ", assignees=" << controller.assigneeOptions().join(',')
                        << ", groups=" << controller.groupOptions().join(',')
                        << ", versions=" << controller.versionOptions().join(',')
                        << ", services=" << controller.serviceOptions().join(',')
                        << ", recordExclusive=" << recordExclusive
                        << ", calendarExclusive=" << calendarExclusive
                        << ", managementExclusive=" << managementExclusive << '\n';
                    app.exit(7);
                    return;
                }
                app.exit(0);
            });
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
