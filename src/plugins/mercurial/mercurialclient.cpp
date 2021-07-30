/****************************************************************************
**
** Copyright (C) 2016 Brian McGillion
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "mercurialclient.h"
#include "mercurialplugin.h"
#include "constants.h"

#include <coreplugin/idocument.h>

#include <vcsbase/vcscommand.h>
#include <vcsbase/vcsoutputwindow.h>
#include <vcsbase/vcsbaseplugin.h>
#include <vcsbase/vcsbaseeditor.h>
#include <vcsbase/vcsbaseeditorconfig.h>
#include <vcsbase/vcsbasediffeditorcontroller.h>

#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>
#include <QTextStream>
#include <QVariant>

using namespace Core;
using namespace DiffEditor;
using namespace Utils;
using namespace VcsBase;

namespace Mercurial {
namespace Internal  {

class MercurialDiffEditorController : public VcsBaseDiffEditorController
{
public:
    MercurialDiffEditorController(IDocument *document)
        : VcsBaseDiffEditorController(document)
    {
        setDisplayName("Hg Diff");
    }

    void runCommand(const QList<QStringList> &args, QTextCodec *codec = nullptr);
    QStringList addConfigurationArguments(const QStringList &args) const;
};

void MercurialDiffEditorController::runCommand(const QList<QStringList> &args, QTextCodec *codec)
{
    // at this moment, don't ignore any errors
    VcsBaseDiffEditorController::runCommand(args, 0u, codec);
}

QStringList MercurialDiffEditorController::addConfigurationArguments(const QStringList &args) const
{
    QStringList configArgs{ "-g", "-p" };
    configArgs << "-U" << QString::number(contextLineCount());
    if (ignoreWhitespace()) {
        configArgs << "-w" << "-b" << "-B" << "-Z";
    }
    return args + configArgs;
}

/////////////////////////////////////////////////////////////

MercurialClient::MercurialClient(MercurialSettings *settings) : VcsBaseClient(settings)
{
}

bool MercurialClient::manifestSync(const FilePath &repository, const QString &relativeFilename)
{
    // This  only works when called from the repo and outputs paths relative to it.
    const QStringList args(QLatin1String("manifest"));

    QtcProcess proc;
    vcsFullySynchronousExec(proc, repository, args);

    const QDir repositoryDir(repository.toString());
    const QFileInfo needle = QFileInfo(repositoryDir, relativeFilename);

    const QStringList files = proc.stdOut().split(QLatin1Char('\n'));
    foreach (const QString &fileName, files) {
        const QFileInfo managedFile(repositoryDir, fileName);
        if (needle == managedFile)
            return true;
    }
    return false;
}

//bool MercurialClient::clone(const QString &directory, const QString &url)
bool MercurialClient::synchronousClone(const FilePath &workingDirectory,
                                       const QString &srcLocation,
                                       const QString &dstLocation,
                                       const QStringList &extraOptions)
{
    Q_UNUSED(extraOptions)
    const unsigned flags = VcsCommand::SshPasswordPrompt |
            VcsCommand::ShowStdOut |
            VcsCommand::ShowSuccessMessage;

    if (workingDirectory.exists()) {
        // Let's make first init
        QStringList arguments(QLatin1String("init"));
        QtcProcess proc;
        vcsFullySynchronousExec(proc, workingDirectory, arguments);
        if (proc.result() != QtcProcess::FinishedWithSuccess)
            return false;

        // Then pull remote repository
        arguments.clear();
        arguments << QLatin1String("pull") << dstLocation;
        QtcProcess proc1;
        vcsSynchronousExec(proc1, workingDirectory, arguments, flags);
        if (proc1.result() != QtcProcess::FinishedWithSuccess)
            return false;

        // By now, there is no hgrc file -> create it
        FileSaver saver(workingDirectory.pathAppended(".hg/hgrc"));
        const QString hgrc = QLatin1String("[paths]\ndefault = ") + dstLocation + QLatin1Char('\n');
        saver.write(hgrc.toUtf8());
        if (!saver.finalize()) {
            VcsOutputWindow::appendError(saver.errorString());
            return false;
        }

        // And last update repository
        arguments.clear();
        arguments << QLatin1String("update");
        QtcProcess proc2;
        vcsSynchronousExec(proc2, workingDirectory, arguments, flags);
        return proc2.result() == QtcProcess::FinishedWithSuccess;
    } else {
        QStringList arguments(QLatin1String("clone"));
        arguments << dstLocation << workingDirectory.parentDir().toString();
        QtcProcess proc;
        vcsSynchronousExec(proc, workingDirectory.parentDir(), arguments, flags);
        return proc.result() == QtcProcess::FinishedWithSuccess;
    }
}

bool MercurialClient::synchronousPull(const FilePath &workingDir, const QString &srcLocation, const QStringList &extraOptions)
{
    QStringList args;
    args << vcsCommandString(PullCommand) << extraOptions << srcLocation;
    // Disable UNIX terminals to suppress SSH prompting
    const unsigned flags =
            VcsCommand::SshPasswordPrompt
            | VcsCommand::ShowStdOut
            | VcsCommand::ShowSuccessMessage;

    // cause mercurial doesn`t understand LANG
    Environment env = Environment::systemEnvironment();
    env.set("LANGUAGE", "C");
    QtcProcess proc;
    proc.setTimeoutS(vcsTimeoutS());

    VcsCommand command(workingDir, env);
    command.addFlags(flags);
    command.runCommand(proc, {vcsBinary(), args});

    const bool ok = proc.result() == QtcProcess::FinishedWithSuccess;

    parsePullOutput(proc.stdOut().trimmed());
    return ok;
}

QString MercurialClient::branchQuerySync(const QString &repositoryRoot)
{
    QFile branchFile(repositoryRoot + QLatin1String("/.hg/branch"));
    if (branchFile.open(QFile::ReadOnly)) {
        const QByteArray branch = branchFile.readAll().trimmed();
        if (!branch.isEmpty())
            return QString::fromLocal8Bit(branch);
    }
    return QLatin1String("Unknown Branch");
}

static QString msgParentRevisionFailed(const FilePath &workingDirectory,
                                       const QString &revision,
                                       const QString &why)
{
    return MercurialClient::tr("Unable to find parent revisions of %1 in %2: %3").
            arg(revision, workingDirectory.toUserOutput(), why);
}

static inline QString msgParseParentsOutputFailed(const QString &output)
{
    return MercurialClient::tr("Cannot parse output: %1").arg(output);
}

QStringList MercurialClient::parentRevisionsSync(const FilePath &workingDirectory,
                                                 const QString &file /* = QString() */,
                                                 const QString &revision)
{
    QStringList parents;
    QStringList args;
    args << QLatin1String("parents") <<  QLatin1String("-r") <<revision;
    if (!file.isEmpty())
        args << file;
    QtcProcess proc;
    vcsFullySynchronousExec(proc, workingDirectory, args);
    if (proc.result() != QtcProcess::FinishedWithSuccess)
        return QStringList();
    /* Looks like: \code
changeset:   0:031a48610fba
user: ...
\endcode   */
    // Obtain first line and split by blank-delimited tokens
    const QStringList lines = proc.stdOut().split(QLatin1Char('\n'));
    if (lines.size() < 1) {
        VcsOutputWindow::appendSilently(
            msgParentRevisionFailed(workingDirectory, revision, msgParseParentsOutputFailed(proc.stdOut())));
        return QStringList();
    }
    QStringList changeSets = lines.front().simplified().split(QLatin1Char(' '));
    if (changeSets.size() < 2) {
        VcsOutputWindow::appendSilently(
            msgParentRevisionFailed(workingDirectory, revision, msgParseParentsOutputFailed(proc.stdOut())));
        return QStringList();
    }
    // Remove revision numbers
    const QChar colon = QLatin1Char(':');
    const QStringList::iterator end = changeSets.end();
    QStringList::iterator it = changeSets.begin();
    for (++it; it != end; ++it) {
        const int colonIndex = it->indexOf(colon);
        if (colonIndex != -1)
            parents.push_back(it->mid(colonIndex + 1));
    }
    return parents;
}

// Describe a change using an optional format
QString MercurialClient::shortDescriptionSync(const FilePath &workingDirectory,
                                              const QString &revision,
                                              const QString &format)
{
    QStringList args;
    args << QLatin1String("log") <<  QLatin1String("-r") <<revision;
    if (!format.isEmpty())
        args << QLatin1String("--template") << format;

    QtcProcess proc;
    vcsFullySynchronousExec(proc, workingDirectory, args);
    if (proc.result() != QtcProcess::FinishedWithSuccess)
        return revision;
    return stripLastNewline(proc.stdOut());
}

// Default format: "SHA1 (author summmary)"
static const char defaultFormatC[] = "{node} ({author|person} {desc|firstline})";

QString MercurialClient::shortDescriptionSync(const FilePath &workingDirectory,
                                              const QString &revision)
{
    return shortDescriptionSync(workingDirectory, revision, QLatin1String(defaultFormatC));
}

bool MercurialClient::managesFile(const FilePath &workingDirectory, const QString &fileName) const
{
    QStringList args;
    args << QLatin1String("status") << QLatin1String("--unknown") << fileName;
    QtcProcess proc;
    vcsFullySynchronousExec(proc, workingDirectory, args);
    return proc.stdOut().isEmpty();
}

void MercurialClient::incoming(const FilePath &repositoryRoot, const QString &repository)
{
    QStringList args;
    args << QLatin1String("incoming") << QLatin1String("-g") << QLatin1String("-p");
    if (!repository.isEmpty())
        args.append(repository);

    QString id = repositoryRoot.toString();
    if (!repository.isEmpty())
        id += QLatin1Char('/') + repository;

    const QString title = tr("Hg incoming %1").arg(id);

    VcsBaseEditorWidget *editor = createVcsEditor(Constants::DIFFLOG_ID, title, repositoryRoot.toString(),
                                                  VcsBaseEditor::getCodec(repositoryRoot.toString()),
                                                  "incoming", id);
    VcsCommand *cmd = createCommand(FilePath::fromString(repository), editor);
    enqueueJob(cmd, args);
}

void MercurialClient::outgoing(const FilePath &repositoryRoot)
{
    QStringList args;
    args << QLatin1String("outgoing") << QLatin1String("-g") << QLatin1String("-p");

    const QString title = tr("Hg outgoing %1").arg(repositoryRoot.toUserOutput());

    VcsBaseEditorWidget *editor = createVcsEditor(Constants::DIFFLOG_ID, title, repositoryRoot.toString(),
                                                  VcsBaseEditor::getCodec(repositoryRoot.toString()),
                                                  "outgoing", repositoryRoot.toString());

    VcsCommand *cmd = createCommand(repositoryRoot, editor);
    enqueueJob(cmd, args);
}

VcsBaseEditorWidget *MercurialClient::annotate(
        const FilePath &workingDir, const QString &file, const QString &revision,
        int lineNumber, const QStringList &extraOptions)
{
    QStringList args(extraOptions);
    args << QLatin1String("-u") << QLatin1String("-c") << QLatin1String("-d");
    return VcsBaseClient::annotate(workingDir, file, revision, lineNumber, args);
}

void MercurialClient::commit(const FilePath &repositoryRoot, const QStringList &files,
                             const QString &commitMessageFile,
                             const QStringList &extraOptions)
{
    QStringList args(extraOptions);
    args << QLatin1String("--noninteractive") << QLatin1String("-l") << commitMessageFile << QLatin1String("-A");
    VcsBaseClient::commit(repositoryRoot, files, commitMessageFile, args);
}

void MercurialClient::diff(const FilePath &workingDir, const QStringList &files,
                           const QStringList &extraOptions)
{
    Q_UNUSED(extraOptions)

    QString fileName;

    if (files.empty()) {
        const QString title = tr("Mercurial Diff");
        const QString sourceFile = VcsBaseEditor::getSource(workingDir, fileName);
        const QString documentId = QString(Constants::MERCURIAL_PLUGIN)
                + ".DiffRepo." + sourceFile;
        requestReload(documentId, sourceFile, title, workingDir.toString(), {"diff"});
    } else if (files.size() == 1) {
        fileName = files.at(0);
        const QString title = tr("Mercurial Diff \"%1\"").arg(fileName);
        const QString sourceFile = VcsBaseEditor::getSource(workingDir, fileName);
        const QString documentId = QString(Constants::MERCURIAL_PLUGIN)
                + ".DiffFile." + sourceFile;
        requestReload(documentId, sourceFile, title, workingDir.toString(), {"diff", fileName});
    } else {
        const QString title = tr("Mercurial Diff \"%1\"").arg(workingDir.toString());
        const QString sourceFile = VcsBaseEditor::getSource(workingDir, fileName);
        const QString documentId = QString(Constants::MERCURIAL_PLUGIN)
                + ".DiffFile." + workingDir.toString();
        requestReload(documentId, sourceFile, title, workingDir.toString(), QStringList{"diff"} + files);
    }
}

void MercurialClient::import(const FilePath &repositoryRoot, const QStringList &files,
                             const QStringList &extraOptions)
{
    VcsBaseClient::import(repositoryRoot, files,
                          QStringList(extraOptions) << QLatin1String("--no-commit"));
}

void MercurialClient::revertAll(const FilePath &workingDir, const QString &revision,
                                const QStringList &extraOptions)
{
    VcsBaseClient::revertAll(workingDir, revision,
                             QStringList(extraOptions) << QLatin1String("--all"));
}

bool MercurialClient::isVcsDirectory(const FilePath &filePath) const
{
    return filePath.isDir()
            && !filePath.fileName().compare(Constants::MERCURIALREPO, HostOsInfo::fileNameCaseSensitivity());
}

void MercurialClient::view(const QString &source, const QString &id,
                           const QStringList &extraOptions)
{
    QStringList args;
    args << QLatin1String("-v") << QLatin1String("log")
         << QLatin1String("-p") << QLatin1String("-g");
    VcsBaseClient::view(source, id, args << extraOptions);
}

FilePath MercurialClient::findTopLevelForFile(const FilePath &file) const
{
    const QString repositoryCheckFile = QLatin1String(Constants::MERCURIALREPO) + QLatin1String("/requires");
    return VcsBase::findRepositoryForFile(file, repositoryCheckFile);
}

Utils::Id MercurialClient::vcsEditorKind(VcsCommandTag cmd) const
{
    switch (cmd) {
    case AnnotateCommand:
        return Constants::ANNOTATELOG_ID;
    case DiffCommand:
        return Constants::DIFFLOG_ID;
    case LogCommand:
        return Constants::FILELOG_ID;
    default:
        return Utils::Id();
    }
}

QStringList MercurialClient::revisionSpec(const QString &revision) const
{
    QStringList args;
    if (!revision.isEmpty())
        args << QLatin1String("-r") << revision;
    return args;
}

MercurialClient::StatusItem MercurialClient::parseStatusLine(const QString &line) const
{
    StatusItem item;
    if (!line.isEmpty())
    {
        if (line.startsWith(QLatin1Char('M')))
            item.flags = QLatin1String("Modified");
        else if (line.startsWith(QLatin1Char('A')))
            item.flags = QLatin1String("Added");
        else if (line.startsWith(QLatin1Char('R')))
            item.flags = QLatin1String("Removed");
        else if (line.startsWith(QLatin1Char('!')))
            item.flags = QLatin1String("Deleted");
        else if (line.startsWith(QLatin1Char('?')))
            item.flags = QLatin1String("Untracked");
        else
            return item;

        //the status line should be similar to "M file_with_changes"
        //so just should take the file name part and store it
        item.file = QDir::fromNativeSeparators(line.mid(2));
    }
    return item;
}

void MercurialClient::requestReload(const QString &documentId, const QString &source, const QString &title,
                                    const QString &workingDirectory, const QStringList &args)
{
    // Creating document might change the referenced source. Store a copy and use it.
    const QString sourceCopy = source;

    IDocument *document = DiffEditorController::findOrCreateDocument(documentId, title);
    QTC_ASSERT(document, return);
    auto controller = new MercurialDiffEditorController(document);
    controller->setReloader([controller, args] {
        controller->runCommand({controller->addConfigurationArguments(args)});
    });
    controller->setVcsBinary(settings().binaryPath.filePath());
    controller->setVcsTimeoutS(settings().timeout.value());
    controller->setProcessEnvironment(processEnvironment());
    controller->setWorkingDirectory(workingDirectory);

    VcsBase::setSource(document, sourceCopy);
    EditorManager::activateEditorForDocument(document);
    controller->requestReload();
}

void MercurialClient::parsePullOutput(const QString &output)
{
    if (output.endsWith(QLatin1String("no changes found")))
        return;

    if (output.endsWith(QLatin1String("(run 'hg update' to get a working copy)"))) {
        emit needUpdate();
        return;
    }

    if (output.endsWith(QLatin1String("'hg merge' to merge)")))
        emit needMerge();
}

} // namespace Internal
} // namespace Mercurial
