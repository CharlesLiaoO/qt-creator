// Copyright (C) 2018 Sergey Morozov
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcheckconstants.h"
#include "cppcheckdiagnosticview.h"
#include "cppchecksettings.h"
#include "cppchecktextmarkmanager.h"
#include "cppchecktool.h"
#include "cppchecktr.h"
#include "cppchecktrigger.h"
#include "cppcheckdiagnosticsmodel.h"
#include "cppcheckmanualrundialog.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>

#include <debugger/analyzer/analyzerconstants.h>
#include <debugger/debuggermainwindow.h>

#include <extensionsystem/iplugin.h>

#include <projectexplorer/kitaspects.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>
#include <utils/utilsicons.h>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace Cppcheck::Internal {

class CppcheckPluginPrivate final : public QObject
{
public:
    explicit CppcheckPluginPrivate();

    CppcheckTextMarkManager marks;
    CppcheckTool tool{marks, Constants::CHECK_PROGRESS_ID};
    CppcheckTrigger trigger{marks, tool};
    DiagnosticsModel manualRunModel;
    CppcheckTool manualRunTool{manualRunModel, Constants::MANUAL_CHECK_PROGRESS_ID};
    Utils::Perspective perspective{Constants::PERSPECTIVE_ID, ::Cppcheck::Tr::tr("Cppcheck")};

    Action *manualRunAction = nullptr;

    void startManualRun();
    void updateManualRunAction();
};

CppcheckPluginPrivate::CppcheckPluginPrivate()
{
    tool.updateOptions(settings());
    connect(&settings(), &AspectContainer::changed, this, [this] {
        tool.updateOptions(settings());
        trigger.recheck();
    });

    auto manualRunView = new DiagnosticView;
    manualRunView->setModel(&manualRunModel);
    perspective.addWindow(manualRunView, Utils::Perspective::SplitVertical, nullptr);

    {
        // Go to previous diagnostic
        auto action = new QAction(this);
        action->setEnabled(false);
        action->setIcon(Utils::Icons::PREV_TOOLBAR.icon());
        action->setToolTip(Tr::tr("Go to previous diagnostic."));
        connect(action, &QAction::triggered,
                manualRunView, &Debugger::DetailedErrorView::goBack);
        connect (&manualRunModel, &DiagnosticsModel::hasDataChanged,
                action, &QAction::setEnabled);
        perspective.addToolBarAction(action);
    }

    {
        // Go to next diagnostic
        auto action = new QAction(this);
        action->setEnabled(false);
        action->setIcon(Utils::Icons::NEXT_TOOLBAR.icon());
        action->setToolTip(Tr::tr("Go to next diagnostic."));
        connect(action, &QAction::triggered,
                manualRunView, &Debugger::DetailedErrorView::goNext);
        connect (&manualRunModel, &DiagnosticsModel::hasDataChanged,
                action, &QAction::setEnabled);
        perspective.addToolBarAction(action);
    }

    {
        // Clear data
        auto action = new QAction(this);
        action->setEnabled(false);
        action->setIcon(Utils::Icons::CLEAN_TOOLBAR.icon());
        action->setToolTip(Tr::tr("Clear"));
        connect(action, &QAction::triggered,
                &manualRunModel, &DiagnosticsModel::clear);
        connect (&manualRunModel, &DiagnosticsModel::hasDataChanged,
                action, &QAction::setEnabled);
        perspective.addToolBarAction(action);
    }
}

void CppcheckPluginPrivate::startManualRun()
{
    auto project = ProjectExplorer::ProjectManager::startupProject();
    if (!project)
        return;

    ManualRunDialog dialog(project);
    if (dialog.exec() == ManualRunDialog::Rejected)
        return;

    manualRunModel.clear();

    const auto files = dialog.filePaths();
    if (files.isEmpty())
        return;

    manualRunTool.setProject(project);
    manualRunTool.updateOptions(dialog.manualRunSettings());
    manualRunTool.check(files);
    perspective.select();
}

void CppcheckPluginPrivate::updateManualRunAction()
{
    const Project *project = ProjectManager::startupProject();
    const Target *target = ProjectManager::startupTarget();
    const Utils::Id cxx = ProjectExplorer::Constants::CXX_LANGUAGE_ID;
    const bool canRun = target && project->projectLanguages().contains(cxx)
                  && ToolchainKitAspect::cxxToolchain(target->kit());
    manualRunAction->setEnabled(canRun);
}

class CppcheckPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Cppcheck.json")

    void initialize() final
    {
        d.reset(new CppcheckPluginPrivate);

        ActionBuilder(this, Constants::MANUAL_RUN_ACTION)
            .setText(Tr::tr("Cppcheck..."))
            .bindContextAction(&d->manualRunAction)
            .addToContainer(Debugger::Constants::M_DEBUG_ANALYZER,
                            Debugger::Constants::G_ANALYZER_TOOLS)
            .addOnTriggered(d.get(), &CppcheckPluginPrivate::startManualRun);

        connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::runActionsUpdated,
                d.get(), &CppcheckPluginPrivate::updateManualRunAction);
        d->updateManualRunAction();
    }

    std::unique_ptr<CppcheckPluginPrivate> d;
};

} // Cppcheck::Internal

#include "cppcheckplugin.moc"
