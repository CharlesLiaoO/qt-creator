// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "compilationdatabaseproject.h"

#include "compilationdatabaseconstants.h"
#include "compilationdbparser.h"

#include <coreplugin/coreplugintr.h>
#include <coreplugin/icontext.h>

#include <cppeditor/projectinfo.h>

#include <projectexplorer/buildinfo.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/deploymentdata.h>
#include <projectexplorer/gcctoolchain.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/kitaspects.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/namedwidget.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/projectupdater.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchainmanager.h>

#include <texteditor/textdocument.h>

#include <utils/algorithm.h>
#include <utils/filesystemwatcher.h>
#include <utils/qtcassert.h>

#include <QFileDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace ProjectExplorer;
using namespace Utils;

namespace CompilationDatabaseProjectManager::Internal {

namespace {

bool isGccCompiler(const QString &compilerName)
{
    return compilerName.contains("gcc")
           || (compilerName.contains("g++") && !compilerName.contains("clang"));
}

bool isClCompatibleCompiler(const QString &compilerName)
{
    return compilerName.endsWith("cl");
}

Utils::Id getCompilerId(QString compilerName)
{
    if (Utils::HostOsInfo::isWindowsHost()) {
        if (compilerName.endsWith(".exe"))
            compilerName.chop(4);
        if (isGccCompiler(compilerName))
            return ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID;
        if (isClCompatibleCompiler(compilerName))
            return ProjectExplorer::Constants::CLANG_CL_TOOLCHAIN_TYPEID;
        return ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID;
    }
    if (isGccCompiler(compilerName))
        return ProjectExplorer::Constants::GCC_TOOLCHAIN_TYPEID;

    // Default is clang
    return ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID;
}

Toolchain *toolchainFromCompilerId(const Utils::Id &compilerId, const Utils::Id &language)
{
    return ToolchainManager::toolchain([&compilerId, &language](const Toolchain *tc) {
        if (!tc->isValid() || tc->language() != language)
            return false;
        return tc->typeId() == compilerId;
    });
}

QString compilerPath(QString pathFlag)
{
    if (pathFlag.isEmpty())
        return pathFlag;
#ifdef Q_OS_WIN
    // Handle short DOS style file names (cmake can generate them).
    const DWORD pathLength = GetLongPathNameW(reinterpret_cast<LPCWSTR>(pathFlag.utf16()),
                                              nullptr,
                                              0);
    if (pathLength > 0) {
        // Works only with existing paths.
        wchar_t *buffer = new wchar_t[pathLength];
        GetLongPathNameW(reinterpret_cast<LPCWSTR>(pathFlag.utf16()), buffer, pathLength);
        pathFlag = QString::fromUtf16(reinterpret_cast<ushort *>(buffer),
                                      static_cast<int>(pathLength - 1));
        delete[] buffer;
    }
#endif
    return QDir::fromNativeSeparators(pathFlag);
}

Toolchain *toolchainFromFlags(const Kit *kit, const QStringList &flags, const Utils::Id &language)
{
    if (flags.empty())
        return ToolchainKitAspect::toolchain(kit, language);

    // Try exact compiler match.
    const Utils::FilePath compiler = Utils::FilePath::fromUserInput(compilerPath(flags.front()));
    Toolchain *toolchain = ToolchainManager::toolchain([&compiler, &language](const Toolchain *tc) {
        return tc->isValid() && tc->language() == language && tc->compilerCommand() == compiler;
    });
    if (toolchain)
        return toolchain;

    Utils::Id compilerId = getCompilerId(compiler.fileName());
    if ((toolchain = toolchainFromCompilerId(compilerId, language)))
        return toolchain;

    if (compilerId != ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID &&
            compilerId != ProjectExplorer::Constants::CLANG_CL_TOOLCHAIN_TYPEID) {
        compilerId = Utils::HostOsInfo::isWindowsHost()
                ? ProjectExplorer::Constants::CLANG_CL_TOOLCHAIN_TYPEID
                : ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID;
        if ((toolchain = toolchainFromCompilerId(compilerId, language)))
            return toolchain;
    }

    toolchain = ToolchainKitAspect::toolchain(kit, language);
    qWarning() << "No matching toolchain found, use the default.";
    return toolchain;
}

void addDriverModeFlagIfNeeded(const Toolchain *toolchain,
                               QStringList &flags,
                               const QStringList &originalFlags)
{
    if (toolchain->typeId() == ProjectExplorer::Constants::CLANG_CL_TOOLCHAIN_TYPEID
        && !originalFlags.empty() && !originalFlags.front().endsWith("cl")
        && !originalFlags.front().endsWith("cl.exe")) {
        flags.prepend("--driver-mode=g++");
    }
}

RawProjectPart makeRawProjectPart(const Utils::FilePath &projectFile,
                                  Kit *kit,
                                  ProjectExplorer::KitInfo &kitInfo,
                                  const FilePath &workingDir,
                                  const FilePath &filePath,
                                  QStringList flags)
{
    HeaderPaths headerPaths;
    Macros macros;
    CppEditor::ProjectFile::Kind fileKind = CppEditor::ProjectFile::Unclassified;

    const QStringList originalFlags = flags;
    filteredFlags(filePath,
                  workingDir,
                  flags,
                  headerPaths,
                  macros,
                  fileKind,
                  kitInfo.sysRootPath);

    RawProjectPart rpp;

    rpp.setProjectFileLocation(projectFile.toString());
    rpp.setBuildSystemTarget(workingDir.path());
    rpp.setDisplayName(filePath.fileName());
    rpp.setFiles({filePath.toFSPathString()});

    rpp.setHeaderPaths(headerPaths);
    rpp.setMacros(macros);

    if (fileKind == CppEditor::ProjectFile::Kind::CHeader
            || fileKind == CppEditor::ProjectFile::Kind::CSource) {
        if (!kitInfo.cToolchain) {
            kitInfo.cToolchain = toolchainFromFlags(kit,
                                                    originalFlags,
                                                    ProjectExplorer::Constants::C_LANGUAGE_ID);
        }
        addDriverModeFlagIfNeeded(kitInfo.cToolchain, flags, originalFlags);
        rpp.setFlagsForC({kitInfo.cToolchain, flags, workingDir});
    } else {
        if (!kitInfo.cxxToolchain) {
            kitInfo.cxxToolchain = toolchainFromFlags(kit,
                                                      originalFlags,
                                                      ProjectExplorer::Constants::CXX_LANGUAGE_ID);
        }
        addDriverModeFlagIfNeeded(kitInfo.cxxToolchain, flags, originalFlags);
        rpp.setFlagsForCxx({kitInfo.cxxToolchain, flags, workingDir});
    }

    return rpp;
}

QStringList relativeDirsList(Utils::FilePath currentPath, const Utils::FilePath &rootPath)
{
    QStringList dirsList;
    while (!currentPath.isEmpty() && currentPath != rootPath) {
        QString dirName = currentPath.fileName();
        if (dirName.isEmpty())
            dirName = currentPath.toString();
        dirsList.prepend(dirName);
        currentPath = currentPath.parentDir();
    }
    return dirsList;
}

FolderNode *addChildFolderNode(FolderNode *parent, const QString &childName)
{
    const Utils::FilePath path = parent->filePath().pathAppended(childName);
    auto node = std::make_unique<FolderNode>(path);
    FolderNode *childNode = node.get();
    childNode->setDisplayName(childName);
    parent->addNode(std::move(node));

    return childNode;
}

FolderNode *addOrGetChildFolderNode(FolderNode *parent, const QString &childName)
{
    FolderNode *fn = parent->findChildFolderNode([&](FolderNode *folder) {
        return folder->filePath().fileName() == childName;
    });
    return fn ? fn : addChildFolderNode(parent, childName);
}

    // Return the node for folderPath.
FolderNode *createFoldersIfNeeded(FolderNode *root, const Utils::FilePath &folderPath)
{
    const QStringList dirsList = relativeDirsList(folderPath, root->filePath());

    FolderNode *parent = root;
    for (const QString &dir : dirsList)
        parent = addOrGetChildFolderNode(parent, dir);

    return parent;
}

FileType fileTypeForName(const QString &fileName)
{
    CppEditor::ProjectFile::Kind fileKind = CppEditor::ProjectFile::classify(fileName);
    if (CppEditor::ProjectFile::isHeader(fileKind))
        return FileType::Header;
    return FileType::Source;
}

void addChild(FolderNode *root, const Utils::FilePath &fileName)
{
    FolderNode *parentNode = createFoldersIfNeeded(root, fileName.parentDir());
    if (!parentNode->fileNode(fileName)) {
        parentNode->addNode(
            std::make_unique<FileNode>(fileName, fileTypeForName(fileName.fileName())));
    }
}

void createTree(std::unique_ptr<ProjectNode> &root,
                const Utils::FilePath &rootPath,
                const RawProjectParts &rpps,
                const QList<FileNode *> &scannedFiles = QList<FileNode *>())
{
    root->setAbsoluteFilePathAndLine(rootPath, -1);
    std::unique_ptr<FolderNode> secondRoot;

    for (const RawProjectPart &rpp : rpps) {
        for (const QString &filePath : rpp.files) {
            Utils::FilePath fileName = Utils::FilePath::fromString(filePath);
            if (!fileName.isChildOf(rootPath)) {
                if (fileName.isChildOf(Utils::FilePath::fromString(rpp.buildSystemTarget))) {
                    if (!secondRoot)
                        secondRoot = std::make_unique<ProjectNode>(
                            Utils::FilePath::fromString(rpp.buildSystemTarget));
                    addChild(secondRoot.get(), fileName);
                }
            } else {
                addChild(root.get(), fileName);
            }
        }
    }

    for (FileNode *node : scannedFiles) {
        if (node->fileType() != FileType::Header)
            continue;

        const Utils::FilePath fileName = node->filePath();
        if (!fileName.isChildOf(rootPath))
            continue;
        FolderNode *parentNode = createFoldersIfNeeded(root.get(), fileName.parentDir());
        if (!parentNode->fileNode(fileName)) {
            std::unique_ptr<FileNode> headerNode(node->clone());
            headerNode->setEnabled(false);
            parentNode->addNode(std::move(headerNode));
        }
    }
    qDeleteAll(scannedFiles);

    if (secondRoot) {
        std::unique_ptr<ProjectNode> firstRoot = std::move(root);
        root = std::make_unique<ProjectNode>(firstRoot->filePath());
        firstRoot->setDisplayName(rootPath.fileName());
        root->addNode(std::move(firstRoot));
        root->addNode(std::move(secondRoot));
    }
}


} // anonymous namespace

CompilationDatabaseBuildSystem::CompilationDatabaseBuildSystem(Target *target)
    : BuildSystem(target)
    , m_cppCodeModelUpdater(ProjectUpdaterFactory::createCppProjectUpdater())
    , m_deployFileWatcher(new FileSystemWatcher(this))
{
    connect(target->project(), &CompilationDatabaseProject::rootProjectDirectoryChanged,
            this, [this] {
        m_projectFileHash.clear();
        requestDelayedParse();
    });

    requestDelayedParse();

    connect(project(), &Project::projectFileIsDirty, this, &CompilationDatabaseBuildSystem::reparseProject);

    connect(m_deployFileWatcher, &FileSystemWatcher::fileChanged,
            this, &CompilationDatabaseBuildSystem::updateDeploymentData);
    connect(target->project(), &Project::activeTargetChanged,
            this, &CompilationDatabaseBuildSystem::updateDeploymentData);
}

CompilationDatabaseBuildSystem::~CompilationDatabaseBuildSystem() = default;

void CompilationDatabaseBuildSystem::triggerParsing()
{
    reparseProject();
}

void CompilationDatabaseBuildSystem::buildTreeAndProjectParts()
{
    Kit *k = kit();
    ProjectExplorer::KitInfo kitInfo(k);
    QTC_ASSERT(kitInfo.isValid(), return);
    // Reset toolchains to pick them based on the database entries.
    kitInfo.cToolchain = nullptr;
    kitInfo.cxxToolchain = nullptr;
    RawProjectParts rpps;

    QTC_ASSERT(m_parser, return);
    const DbContents dbContents = m_parser->dbContents();
    const DbEntry *prevEntry = nullptr;
    for (const DbEntry &entry : dbContents.entries) {
        if (prevEntry && prevEntry->flags == entry.flags) {
            rpps.back().files.append(entry.fileName.toString());
            continue;
        }

        prevEntry = &entry;

        RawProjectPart rpp = makeRawProjectPart(projectFilePath(),
                                                k,
                                                kitInfo,
                                                entry.workingDir,
                                                entry.fileName,
                                                entry.flags);

        rpps.append(rpp);
    }

    if (!dbContents.extras.empty()) {
        const Utils::FilePath baseDir = projectFilePath().parentDir();

        QStringList extraFiles;
        for (const QString &extra : dbContents.extras)
            extraFiles.append(baseDir.pathAppended(extra).toString());

        RawProjectPart rppExtra;
        rppExtra.setFiles(extraFiles);
        rpps.append(rppExtra);
    }


    auto root = std::make_unique<ProjectNode>(projectDirectory());
    createTree(root, project()->rootProjectDirectory(), rpps, m_parser->scannedFiles());

    root->addNode(std::make_unique<FileNode>(projectFilePath(), FileType::Project));

    if (QFileInfo::exists(dbContents.extraFileName))
        root->addNode(std::make_unique<FileNode>(Utils::FilePath::fromString(dbContents.extraFileName),
                                                 FileType::Project));

    setRootProjectNode(std::move(root));

    m_cppCodeModelUpdater->update({project(), kitInfo, activeParseEnvironment(), rpps});
    updateDeploymentData();
}

CompilationDatabaseProject::CompilationDatabaseProject(const Utils::FilePath &projectFile)
    : Project(Constants::COMPILATIONDATABASEMIMETYPE, projectFile)
{
    setId(Constants::COMPILATIONDATABASEPROJECT_ID);
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::CXX_LANGUAGE_ID));
    setDisplayName(projectDirectory().fileName());
    setBuildSystemCreator([](Target *t) { return new CompilationDatabaseBuildSystem(t); });
    setExtraProjectFiles(
        {projectFile.stringAppended(Constants::COMPILATIONDATABASEPROJECT_FILES_SUFFIX)});
}

Utils::FilePath CompilationDatabaseProject::rootPathFromSettings() const
{
    Utils::FilePath rootPath;
#ifndef WITH_TESTS
    rootPath = Utils::FilePath::fromString(
        namedSettings(ProjectExplorer::Constants::PROJECT_ROOT_PATH_KEY).toString());
#endif
    if (rootPath.isEmpty())
        rootPath = projectDirectory();
    return rootPath;
}

void CompilationDatabaseProject::configureAsExampleProject(Kit *kit)
{
    if (kit)
        addTargetForKit(kit);
    else if (KitManager::defaultKit())
        addTargetForKit(KitManager::defaultKit());
}

void CompilationDatabaseBuildSystem::reparseProject()
{
    if (m_parser) {
        QTC_CHECK(isParsing());
        m_parser->stop();
    }
    const FilePath rootPath = static_cast<CompilationDatabaseProject *>(project())->rootPathFromSettings();
    m_parser = new CompilationDbParser(project()->displayName(),
                                       projectFilePath(),
                                       rootPath,
                                       m_mimeBinaryCache,
                                       guardParsingRun(),
                                       this);
    connect(m_parser, &CompilationDbParser::finished, this, [this](ParseResult result) {
        m_projectFileHash = m_parser->projectFileHash();
        if (result == ParseResult::Success)
            buildTreeAndProjectParts();
        m_parser = nullptr;
    });
    m_parser->setPreviousProjectFileHash(m_projectFileHash);
    m_parser->start();
}

void CompilationDatabaseBuildSystem::updateDeploymentData()
{
    const Utils::FilePath deploymentFilePath = projectDirectory()
            .pathAppended("QtCreatorDeployment.txt");
    DeploymentData deploymentData;
    deploymentData.addFilesFromDeploymentFile(deploymentFilePath, projectDirectory());
    setDeploymentData(deploymentData);
    if (m_deployFileWatcher->files() != QStringList(deploymentFilePath.toString())) {
        m_deployFileWatcher->clear();
        m_deployFileWatcher->addFile(deploymentFilePath.toString(),
                                     FileSystemWatcher::WatchModifiedDate);
    }

    emitBuildSystemUpdated();
}

static TextEditor::TextDocument *createCompilationDatabaseDocument()
{
    auto doc = new TextEditor::TextDocument;
    doc->setId(Constants::COMPILATIONDATABASEPROJECT_ID);
    doc->setMimeType(Constants::COMPILATIONDATABASEMIMETYPE);
    return doc;
}

class CompilationDatabaseEditorFactory final : public TextEditor::TextEditorFactory
{
public:
    CompilationDatabaseEditorFactory()
    {
        setId(Constants::COMPILATIONDATABASEPROJECT_ID);
        setDisplayName(::Core::Tr::tr("Compilation Database"));
        addMimeType(Constants::COMPILATIONDATABASEMIMETYPE);

        setEditorCreator([] { return new TextEditor::BaseTextEditor; });
        setEditorWidgetCreator([] { return new TextEditor::TextEditorWidget; });
        setDocumentCreator(createCompilationDatabaseDocument);
        setUseGenericHighlighter(true);
        setCommentDefinition(Utils::CommentDefinition::HashStyle);
        setCodeFoldingSupported(true);
    }
};

void setupCompilationDatabaseEditor()
{
    static CompilationDatabaseEditorFactory theCompilationDatabaseEditorFactory;
}

// CompilationDatabaseBuildConfigurationFactory

class CompilationDatabaseBuildConfiguration final : public BuildConfiguration
{
public:
    CompilationDatabaseBuildConfiguration(Target *target, Id id)
        : BuildConfiguration(target, id)
    {}
};

class CompilationDatabaseBuildConfigurationFactory final : public BuildConfigurationFactory
{
public:
    CompilationDatabaseBuildConfigurationFactory()
    {
        registerBuildConfiguration<CompilationDatabaseBuildConfiguration>(
            "CompilationDatabase.CompilationDatabaseBuildConfiguration");

        setSupportedProjectType(Constants::COMPILATIONDATABASEPROJECT_ID);
        setSupportedProjectMimeTypeName(Constants::COMPILATIONDATABASEMIMETYPE);

        setBuildGenerator([](const Kit *, const FilePath &projectPath, bool) {
            const QString name = QCoreApplication::translate("QtC::ProjectExplorer", "Release");
            ProjectExplorer::BuildInfo info;
            info.typeName = name;
            info.displayName = name;
            info.buildType = BuildConfiguration::Release;
            info.buildDirectory = projectPath.parentDir();
            return QList<BuildInfo>{info};
        });
    }
};

void setupCompilationDatabaseBuildConfiguration()
{
    static CompilationDatabaseBuildConfigurationFactory theCDBuildConfigurationFactory;
}

} // CompilationDatabaseProjectManager::Internal
