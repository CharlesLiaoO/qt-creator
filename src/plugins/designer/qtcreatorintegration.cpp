/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "qtcreatorintegration.h"
#include "formwindoweditor.h"
#include "formeditorw.h"
#include "editordata.h"
#include <widgethost.h>
#include <designer/cpp/formclasswizardpage.h>

#include <cpptools/cppmodelmanager.h>
#include <cpptools/cpptoolsconstants.h>
#include <cpptools/cppworkingcopy.h>
#include <cpptools/insertionpointlocator.h>
#include <cpptools/symbolfinder.h>
#include <cplusplus/Overview.h>
#include <coreplugin/icore.h>
#include <texteditor/texteditor.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/session.h>
#include <utils/qtcassert.h>

#include <QDesignerFormWindowInterface>
#include <QDesignerFormEditorInterface>

#include <QMessageBox>

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QUrl>

enum { indentation = 4 };

using namespace Designer::Internal;
using namespace CPlusPlus;
using namespace TextEditor;
using namespace ProjectExplorer;

static QString msgClassNotFound(const QString &uiClassName, const QList<Document::Ptr> &docList)
{
    QString files;
    foreach (const Document::Ptr &doc, docList) {
        files += QLatin1Char('\n');
        files += QDir::toNativeSeparators(doc->fileName());
    }
    return QtCreatorIntegration::tr(
        "The class containing \"%1\" could not be found in %2.\n"
        "Please verify the #include-directives.")
        .arg(uiClassName, files);
}

QtCreatorIntegration::QtCreatorIntegration(QDesignerFormEditorInterface *core, QObject *parent)
    : QDesignerIntegration(core, parent)
{
    setResourceFileWatcherBehaviour(ReloadResourceFileSilently);
    Feature f = features();
    f |= SlotNavigationFeature;
    f &= ~ResourceEditorFeature;
    setFeatures(f);
    connect(this, SIGNAL(navigateToSlot(QString,QString,QStringList)),
            this, SLOT(slotNavigateToSlot(QString,QString,QStringList)));
    connect(this, SIGNAL(helpRequested(QString,QString)),
            this, SLOT(slotDesignerHelpRequested(QString,QString)));
    slotSyncSettingsToDesigner();
    connect(Core::ICore::instance(), SIGNAL(saveSettingsRequested()),
            this, SLOT(slotSyncSettingsToDesigner()));
}

void QtCreatorIntegration::slotDesignerHelpRequested(const QString &manual, const QString &document)
{
    // Pass on as URL.
    emit creatorHelpRequested(QUrl(QString::fromLatin1("qthelp://com.trolltech.%1/qdoc/%2")
        .arg(manual, document)));
}

void QtCreatorIntegration::updateSelection()
{
    if (SharedTools::WidgetHost *host = FormEditorW::activeWidgetHost())
        host->updateFormWindowSelectionHandles(true);
    QDesignerIntegration::updateSelection();
}

QWidget *QtCreatorIntegration::containerWindow(QWidget * /*widget*/) const
{
    if (SharedTools::WidgetHost *host = FormEditorW::activeWidgetHost())
        return host->integrationContainer();
    return 0;
}

static QList<Document::Ptr> findDocumentsIncluding(const Snapshot &docTable,
                                                   const QString &fileName, bool checkFileNameOnly)
{
    QList<Document::Ptr> docList;
    foreach (const Document::Ptr &doc, docTable) { // we go through all documents
        const QList<Document::Include> includes = doc->resolvedIncludes()
            + doc->unresolvedIncludes();
        foreach (const Document::Include &include, includes) {
            if (checkFileNameOnly) {
                const QFileInfo fi(include.unresolvedFileName());
                if (fi.fileName() == fileName) { // we are only interested in docs which includes fileName only
                    docList.append(doc);
                }
            } else {
                if (include.resolvedFileName() == fileName)
                    docList.append(doc);
            }
        }
    }
    return docList;
}

// Does klass inherit baseClass?
static bool inherits(const Overview &o, const Class *klass, const QString &baseClass)
{
    const int baseClassCount = klass->baseClassCount();
    for (int b = 0; b < baseClassCount; b++)
        if (o.prettyName(klass->baseClassAt(b)->name()) == baseClass)
            return true;
    return false;
}

QString fullyQualifiedName(const LookupContext &context, const Name *name, Scope *scope)
{
    if (!name || !scope)
        return QString();

    const QList<LookupItem> items = context.lookup(name, scope);
    if (items.isEmpty()) { // "ui_xxx.h" might not be generated and nothing is forward declared.
        return Overview().prettyName(name);
    } else {
        Symbol *symbol = items.first().declaration();
        return Overview().prettyName(LookupContext::fullyQualifiedName(symbol));
    }
    return QString();
}

// Find class definition in namespace (that is, the outer class
// containing a member of the desired class type) or inheriting the desired class
// in case of forms using the Multiple Inheritance approach
static const Class *findClass(const Namespace *parentNameSpace, const LookupContext &context,
                              const QString &className, QString *namespaceName)
{
    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << className;

    const Overview o;
    const unsigned namespaceMemberCount = parentNameSpace->memberCount();
    for (unsigned i = 0; i < namespaceMemberCount; ++i) { // we go through all namespace members
        const Symbol *sym = parentNameSpace->memberAt(i);
        // we have found a class - we are interested in classes only
        if (const Class *cl = sym->asClass()) {
            // 1) we go through class members
            const unsigned classMemberCount = cl->memberCount();
            for (unsigned j = 0; j < classMemberCount; ++j)
                if (Declaration *decl = cl->memberAt(j)->asDeclaration()) {
                // we want to know if the class contains a member (so we look into
                // a declaration) of uiClassName type
                    QString nameToMatch;
                    if (const NamedType *nt = decl->type()->asNamedType()) {
                        nameToMatch = fullyQualifiedName(context, nt->name(),
                                                         decl->enclosingScope());
                    // handle pointers to member variables
                    } else if (PointerType *pt = decl->type()->asPointerType()) {
                        if (NamedType *nt = pt->elementType()->asNamedType()) {
                            nameToMatch = fullyQualifiedName(context, nt->name(),
                                                             decl->enclosingScope());
                        }
                    }
                    if (!nameToMatch.isEmpty() && className == nameToMatch)
                        return cl;
                } // decl
            // 2) does it inherit the desired class
            if (inherits(o, cl, className))
                return cl;
        } else {
            // Check namespaces
            if (const Namespace *ns = sym->asNamespace()) {
                QString tempNS = *namespaceName;
                tempNS += o.prettyName(ns->name());
                tempNS += QLatin1String("::");
                if (const Class *cl = findClass(ns, context, className, &tempNS)) {
                    *namespaceName = tempNS;
                    return cl;
                }
            } // member is namespave
        } // member is no class
    } // for members
    return 0;
}

static Function *findDeclaration(const Class *cl, const QString &functionName)
{
    const QString funName = QString::fromUtf8(QMetaObject::normalizedSignature(functionName.toUtf8()));
    const unsigned mCount = cl->memberCount();
    // we are interested only in declarations (can be decl of function or of a field)
    // we are only interested in declarations of functions
    const Overview overview;
    for (unsigned j = 0; j < mCount; ++j) { // go through all members
        if (Declaration *decl = cl->memberAt(j)->asDeclaration())
            if (Function *fun = decl->type()->asFunctionType()) {
                // Format signature
                QString memberFunction = overview.prettyName(fun->name());
                memberFunction += QLatin1Char('(');
                const uint aCount = fun->argumentCount();
                for (uint i = 0; i < aCount; i++) { // we build argument types string
                    const Argument *arg = fun->argumentAt(i)->asArgument();
                    if (i > 0)
                        memberFunction += QLatin1Char(',');
                    memberFunction += overview.prettyType(arg->type());
                }
                memberFunction += QLatin1Char(')');
                // we compare normalized signatures
                memberFunction = QString::fromUtf8(QMetaObject::normalizedSignature(memberFunction.toUtf8()));
                if (memberFunction == funName) // we match function names and argument lists
                    return fun;
            }
    }
    return 0;
}

// TODO: remove me, this is taken from cppeditor.cpp. Find some common place for this function
static Document::Ptr findDefinition(Function *functionDeclaration, int *line)
{
    if (CppTools::CppModelManager *cppModelManager = CppTools::CppModelManager::instance()) {
        const Snapshot snapshot = cppModelManager->snapshot();
        CppTools::SymbolFinder symbolFinder;
        if (Function *fun = symbolFinder.findMatchingDefinition(functionDeclaration, snapshot)) {
            if (line)
                *line = fun->line();

            return snapshot.document(QString::fromUtf8(fun->fileName(), fun->fileNameLength()));
        }
    }

    return Document::Ptr();
}

static inline BaseTextEditor *editorAt(const QString &fileName, int line, int column)
{
    return qobject_cast<BaseTextEditor *>(Core::EditorManager::openEditorAt(fileName, line, column,
                                                                         Core::Id(),
                                                                         Core::EditorManager::DoNotMakeVisible));
}

static void addDeclaration(const Snapshot &snapshot,
                           const QString &fileName,
                           const Class *cl,
                           const QString &functionName)
{
    QString declaration = QLatin1String("void ");
    declaration += functionName;
    declaration += QLatin1String(";\n");

    CppTools::CppRefactoringChanges refactoring(snapshot);
    CppTools::InsertionPointLocator find(refactoring);
    const CppTools::InsertionLocation loc = find.methodDeclarationInClass(
                fileName, cl, CppTools::InsertionPointLocator::PrivateSlot);

    //
    //! \todo change this to use the Refactoring changes.
    //

    if (BaseTextEditor *editor = editorAt(fileName, loc.line(), loc.column() - 1)) {
        QTextCursor tc = editor->textCursor();
        int pos = tc.position();
        tc.beginEditBlock();
        tc.insertText(loc.prefix() + declaration + loc.suffix());
        tc.setPosition(pos, QTextCursor::KeepAnchor);
        editor->textDocument()->autoIndent(tc);
        tc.endEditBlock();
    }
}

static Document::Ptr addDefinition(const Snapshot &docTable,
                                   const QString &headerFileName,
                                   const QString &className,
                                   const QString &functionName,
                                   int *line)
{
    QString definition = QLatin1String("\nvoid ");
    definition += className;
    definition += QLatin1String("::");
    definition += functionName;
    definition += QLatin1String("\n{\n");
    definition += QString(indentation, QLatin1Char(' '));
    definition += QLatin1String("\n}\n");

    // we find all documents which include headerFileName
    const QList<Document::Ptr> docList = findDocumentsIncluding(docTable, headerFileName, false);
    if (docList.isEmpty())
        return Document::Ptr();

    QFileInfo headerFI(headerFileName);
    const QString headerBaseName = headerFI.completeBaseName();
    foreach (const Document::Ptr &doc, docList) {
        const QFileInfo sourceFI(doc->fileName());
        // we take only those documents which have the same filename
        if (headerBaseName == sourceFI.baseName()) {
            //
            //! \todo change this to use the Refactoring changes.
            //

            if (BaseTextEditor *editor = editorAt(doc->fileName(), 0, 0)) {

                //
                //! \todo use the InsertionPointLocator to insert at the correct place.
                // (we'll have to extend that class first to do definition insertions)

                const QString contents = editor->textDocument()->plainText();
                int column;
                editor->convertPosition(contents.length(), line, &column);
                editor->gotoLine(*line, column);
                editor->insert(definition);
                *line += 1;
            }
            return doc;
        }
    }
    return Document::Ptr();
}

static QString addConstRefIfNeeded(const QString &argument)
{
    if (argument.startsWith(QLatin1String("const "))
            || argument.endsWith(QLatin1Char('&'))
            || argument.endsWith(QLatin1Char('*')))
        return argument;

    // for those types we don't want to add "const &"
    static const QStringList nonConstRefs = QStringList()
            << QLatin1String("bool")
            << QLatin1String("int")
            << QLatin1String("uint")
            << QLatin1String("float")
            << QLatin1String("double")
            << QLatin1String("long")
            << QLatin1String("short")
            << QLatin1String("char")
            << QLatin1String("signed")
            << QLatin1String("unsigned")
            << QLatin1String("qint64")
            << QLatin1String("quint64");

    for (int i = 0; i < nonConstRefs.count(); i++) {
        const QString nonConstRef = nonConstRefs.at(i);
        if (argument == nonConstRef || argument.startsWith(nonConstRef + QLatin1Char(' ')))
            return argument;
    }
    return QLatin1String("const ") + argument + QLatin1Char('&');
}

static QString formatArgument(const QString &argument)
{
    QString formattedArgument = argument;
    int i = argument.count();
    while (i > 0) { // from the end of the "argument" string
        i--;
        const QChar c = argument.at(i); // take the char
        if (c != QLatin1Char('*') && c != QLatin1Char('&')) { // if it's not the * or &
            formattedArgument.insert(i + 1, QLatin1Char(' ')); // insert space after that char or just append space (to separate it from the parameter name)
            break;
        }
    }
    return formattedArgument;
}

// Insert the parameter names into a signature, "void foo(bool)" ->
// "void foo(bool checked)"
static QString addParameterNames(const QString &functionSignature, const QStringList &parameterNames)
{
    const int firstParen = functionSignature.indexOf(QLatin1Char('('));
    QString functionName = functionSignature.left(firstParen + 1);
    QString argumentsString = functionSignature.mid(firstParen + 1);
    const int lastParen = argumentsString.lastIndexOf(QLatin1Char(')'));
    if (lastParen != -1)
        argumentsString.truncate(lastParen);
    const QStringList arguments = argumentsString.split(QLatin1Char(','), QString::SkipEmptyParts);
    const int pCount = parameterNames.count();
    const int aCount = arguments.count();
    for (int i = 0; i < aCount; ++i) {
        if (i > 0)
            functionName += QLatin1String(", ");
        const QString argument = addConstRefIfNeeded(arguments.at(i));
        functionName += formatArgument(argument);
        if (i < pCount) {
            // prepare parameterName
            QString parameterName = parameterNames.at(i);
            if (parameterName.isEmpty()) {
                const QString generatedName = QLatin1String("arg") + QString::number(i + 1);
                if (!parameterNames.contains(generatedName))
                    parameterName = generatedName;
            }
            // add parameterName if not empty
            if (!parameterName.isEmpty())
                functionName += parameterName;
        }
    }
    functionName += QLatin1Char(')');
    return functionName;
}

// Recursively find a class definition in the document passed on or in its
// included files (going down [maxIncludeDepth] includes) and return a pair
// of <Class*, Document>.

typedef QPair<const Class *, Document::Ptr> ClassDocumentPtrPair;

static ClassDocumentPtrPair
        findClassRecursively(const LookupContext &context, const QString &className,
                             unsigned maxIncludeDepth, QString *namespaceName)
{
    const Document::Ptr doc = context.thisDocument();
    const Snapshot docTable = context.snapshot();
    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << doc->fileName() << className << maxIncludeDepth;
    // Check document
    if (const Class *cl = findClass(doc->globalNamespace(), context, className, namespaceName))
        return ClassDocumentPtrPair(cl, doc);
    if (maxIncludeDepth) {
        // Check the includes
        const unsigned recursionMaxIncludeDepth = maxIncludeDepth - 1u;
        foreach (const QString &include, doc->includedFiles()) {
            const Snapshot::const_iterator it = docTable.find(include);
            if (it != docTable.end()) {
                const Document::Ptr includeDoc = it.value();
                LookupContext context(includeDoc, docTable);
                const ClassDocumentPtrPair irc = findClassRecursively(context, className,
                    recursionMaxIncludeDepth, namespaceName);
                if (irc.first)
                    return irc;
            }
        }
    }
    return ClassDocumentPtrPair(0, Document::Ptr());
}

void QtCreatorIntegration::slotNavigateToSlot(const QString &objectName, const QString &signalSignature,
        const QStringList &parameterNames)
{
    QString errorMessage;
    if (!navigateToSlot(objectName, signalSignature, parameterNames, &errorMessage) && !errorMessage.isEmpty())
        QMessageBox::warning(FormEditorW::designerEditor()->topLevel(), tr("Error finding/adding a slot."), errorMessage);
}

// Build name of the class as generated by uic, insert Ui namespace
// "foo::bar::form" -> "foo::bar::Ui::form"

static inline QString uiClassName(QString formObjectName)
{
    const int indexOfScope = formObjectName.lastIndexOf(QLatin1String("::"));
    const int uiNameSpaceInsertionPos = indexOfScope >= 0 ? indexOfScope + 2 : 0;
    formObjectName.insert(uiNameSpaceInsertionPos, QLatin1String("Ui::"));
    return formObjectName;
}

static Document::Ptr getParsedDocument(const QString &fileName,
                                       CppTools::WorkingCopy &workingCopy,
                                       Snapshot &snapshot)
{
    QByteArray src;
    if (workingCopy.contains(fileName)) {
        src = workingCopy.source(fileName);
    } else {
        Utils::FileReader reader;
        if (reader.fetch(fileName)) // ### FIXME error reporting
            src = QString::fromLocal8Bit(reader.data()).toUtf8();
    }

    Document::Ptr doc = snapshot.preprocessedDocument(src, fileName);
    doc->check();
    snapshot.insert(doc);
    return doc;
}

// Goto slot invoked by the designer context menu. Either navigates
// to an existing slot function or create a new one.

bool QtCreatorIntegration::navigateToSlot(const QString &objectName,
                                          const QString &signalSignature,
                                          const QStringList &parameterNames,
                                          QString *errorMessage)
{
    typedef QMap<int, Document::Ptr> DocumentMap;

    const QString currentUiFile = FormEditorW::activeEditor()->document()->filePath();
#if 0
    return Designer::Internal::navigateToSlot(currentUiFile, objectName, signalSignature, parameterNames, errorMessage);
#endif
    // TODO: we should pass to findDocumentsIncluding an absolute path to generated .h file from ui.
    // Currently we are guessing the name of ui_<>.h file and pass the file name only to the findDocumentsIncluding().
    // The idea is that the .pro file knows if the .ui files is inside, and the .pro file knows it will
    // be generating the ui_<>.h file for it, and the .pro file knows what the generated file's name and its absolute path will be.
    // So we should somehow get that info from project manager (?)
    const QFileInfo fi(currentUiFile);
    const QString uiFolder = fi.absolutePath();
    const QString uicedName = QLatin1String("ui_") + fi.completeBaseName() + QLatin1String(".h");

    // Retrieve code model snapshot restricted to project of ui file or the working copy.
    Snapshot docTable = CppTools::CppModelManager::instance()->snapshot();
    Snapshot newDocTable;
    const Project *uiProject = SessionManager::projectForFile(currentUiFile);
    if (uiProject) {
        Snapshot::const_iterator end = docTable.end();
        for (Snapshot::iterator it = docTable.begin(); it != end; ++it) {
            const Project *project = SessionManager::projectForFile(it.key());
            if (project == uiProject)
                newDocTable.insert(it.value());
        }
    } else {
        const CppTools::WorkingCopy workingCopy =
                CppTools::CppModelManager::instance()->workingCopy();
        QHashIterator<QString, QPair<QByteArray, unsigned> > it = workingCopy.iterator();
        while (it.hasNext()) {
            it.next();
            const QString fileName = it.key();
            if (fileName != CppTools::CppModelManager::configurationFileName())
                newDocTable.insert(docTable.document(fileName));
        }
    }
    docTable = newDocTable;

    // take all docs, find the ones that include the ui_xx.h.
    // Sort into a map, putting the ones whose path closely matches the ui-folder path
    // first in case there are project subdirectories that contain identical file names.
    const QList<Document::Ptr> docList = findDocumentsIncluding(docTable, uicedName, true); // change to false when we know the absolute path to generated ui_<>.h file
    DocumentMap docMap;
    foreach (const Document::Ptr &d, docList) {
        const QFileInfo docFi(d->fileName());
        docMap.insert(qAbs(docFi.absolutePath().compare(uiFolder, Qt::CaseInsensitive)), d);
    }

    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << objectName << signalSignature << "Looking for " << uicedName << " returned " << docList.size();
    if (docMap.isEmpty()) {
        *errorMessage = tr("No documents matching \"%1\" could be found.\nRebuilding the project might help.").arg(uicedName);
        return false;
    }

    QDesignerFormWindowInterface *fwi = FormEditorW::activeWidgetHost()->formWindow();

    const QString uiClass = uiClassName(fwi->mainContainer()->objectName());

    if (Designer::Constants::Internal::debug)
        qDebug() << "Checking docs for " << uiClass;

    // Find the class definition (ui class defined as member or base class)
    // in the file itself or in the directly included files (order 1).
    QString namespaceName;
    const Class *cl = 0;
    Document::Ptr doc;

    foreach (const Document::Ptr &d, docMap) {
        LookupContext context(d, docTable);
        const ClassDocumentPtrPair cd = findClassRecursively(context, uiClass, 1u , &namespaceName);
        if (cd.first) {
            cl = cd.first;
            doc = cd.second;
            break;
        }
    }
    if (!cl) {
        *errorMessage = msgClassNotFound(uiClass, docList);
        return false;
    }

    Overview o;
    const QString className = namespaceName + o.prettyName(cl->name());
    if (Designer::Constants::Internal::debug)
        qDebug() << "Found class  " << className << doc->fileName();

    const QString functionName = QLatin1String("on_") + objectName + QLatin1Char('_') + signalSignature;
    const QString functionNameWithParameterNames = addParameterNames(functionName, parameterNames);

    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << "Found " << uiClass << doc->fileName() << " checking " << functionName  << functionNameWithParameterNames;

    int line = 0;
    Document::Ptr sourceDoc;

    if (Function *fun = findDeclaration(cl, functionName)) {
        sourceDoc = findDefinition(fun, &line);
        if (!sourceDoc) {
            // add function definition to cpp file
            sourceDoc = addDefinition(docTable, doc->fileName(), className, functionNameWithParameterNames, &line);
        }
    } else {
        // add function declaration to cl
        CppTools::WorkingCopy workingCopy =
            CppTools::CppModelManager::instance()->workingCopy();
        const QString fileName = doc->fileName();
        getParsedDocument(fileName, workingCopy, docTable);
        addDeclaration(docTable, fileName, cl, functionNameWithParameterNames);

        // add function definition to cpp file
        sourceDoc = addDefinition(docTable, fileName, className, functionNameWithParameterNames, &line);
    }

    if (!sourceDoc) {
        *errorMessage = tr("Unable to add the method definition.");
        return false;
    }

    // jump to function definition, position within code
    Core::EditorManager::openEditorAt(sourceDoc->fileName(), line + 2, indentation);

    return true;
}

void QtCreatorIntegration::slotSyncSettingsToDesigner()
{
    // Set promotion-relevant parameters on integration.
    setHeaderSuffix(Core::MimeDatabase::preferredSuffixByType(QLatin1String(CppTools::Constants::CPP_HEADER_MIMETYPE)));
    setHeaderLowercase(FormClassWizardPage::lowercaseHeaderFiles());
}
