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

#include "plaintexteditorfactory.h"
#include "texteditor.h"
#include "textdocument.h"
#include "normalindenter.h"
#include "texteditoractionhandler.h"
#include "texteditorconstants.h"
#include "texteditorplugin.h"
#include "texteditorsettings.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/infobar.h>
#include <utils/qtcassert.h>

#include <QCoreApplication>

namespace TextEditor {

static PlainTextEditorFactory *m_instance = 0;

class PlainTextEditorWidget : public BaseTextEditorWidget
{
public:
    PlainTextEditorWidget() {}
    void finalizeInitialization() { setupAsPlainEditor(); }
};

PlainTextEditorFactory::PlainTextEditorFactory()
{
    QTC_CHECK(!m_instance);
    m_instance = this;
    setId(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID);
    setDisplayName(qApp->translate("OpenWith::Editors", Core::Constants::K_DEFAULT_TEXT_EDITOR_DISPLAY_NAME));
    addMimeType(QLatin1String(TextEditor::Constants::C_TEXTEDITOR_MIMETYPE_TEXT));

    setDocumentCreator([]() { return new TextDocument(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID); });
    setEditorWidgetCreator([]() { return new PlainTextEditorWidget; });
    setIndenterCreator([]() { return new NormalIndenter; });

    setEditorActionHandlers(TextEditorActionHandler::Format |
        TextEditorActionHandler::UnCommentSelection |
        TextEditorActionHandler::UnCollapseAll);
}

PlainTextEditorFactory *PlainTextEditorFactory::instance()
{
    return m_instance;
}

BaseTextEditor *PlainTextEditorFactory::createPlainTextEditor()
{
    return qobject_cast<BaseTextEditor *>(m_instance->createEditor());
}

} // namespace TextEditor
