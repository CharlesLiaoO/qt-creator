/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "textmark.h"

#include "fontsettings.h"
#include "textdocument.h"
#include "texteditor.h"
#include "texteditorplugin.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/icore.h>
#include <utils/qtcassert.h>
#include <utils/tooltip/tooltip.h>
#include <utils/utilsicons.h>

#include <QAction>
#include <QGridLayout>
#include <QPainter>
#include <QToolButton>

using namespace Core;
using namespace Utils;
using namespace TextEditor::Internal;

namespace TextEditor {

class TextMarkRegistry : public QObject
{
    Q_OBJECT
public:
    static void add(TextMark *mark);
    static bool remove(TextMark *mark);

private:
    TextMarkRegistry(QObject *parent);
    static TextMarkRegistry* instance();
    void editorOpened(Core::IEditor *editor);
    void documentRenamed(Core::IDocument *document,
                         const FilePath &oldPath,
                         const FilePath &newPath);
    void allDocumentsRenamed(const FilePath &oldPath, const FilePath &newPath);

    QHash<Utils::FilePath, QSet<TextMark *> > m_marks;
};

class AnnotationColors
{
public:
    static AnnotationColors &getAnnotationColors(const QColor &markColor,
                                                 const QColor &backgroundColor);

public:
    using SourceColors = QPair<QColor, QColor>;
    QColor rectColor;
    QColor textColor;

private:
    static QHash<SourceColors, AnnotationColors> m_colorCache;
};

TextMarkRegistry *m_instance = nullptr;

TextMark::TextMark(const FilePath &fileName, int lineNumber, Id category, double widthFactor)
    : m_fileName(fileName)
    , m_lineNumber(lineNumber)
    , m_visible(true)
    , m_category(category)
    , m_widthFactor(widthFactor)
{
    if (!m_fileName.isEmpty())
        TextMarkRegistry::add(this);
}

TextMark::~TextMark()
{
    qDeleteAll(m_actions);
    m_actions.clear();
    delete m_settingsAction;
    if (!m_fileName.isEmpty())
        TextMarkRegistry::remove(this);
    if (m_baseTextDocument)
        m_baseTextDocument->removeMark(this);
    m_baseTextDocument = nullptr;
}

FilePath TextMark::fileName() const
{
    return m_fileName;
}

void TextMark::updateFileName(const FilePath &fileName)
{
    if (fileName == m_fileName)
        return;
    if (!m_fileName.isEmpty())
        TextMarkRegistry::remove(this);
    m_fileName = fileName;
    if (!m_fileName.isEmpty())
        TextMarkRegistry::add(this);
}

int TextMark::lineNumber() const
{
    return m_lineNumber;
}

void TextMark::paintIcon(QPainter *painter, const QRect &rect) const
{
    icon().paint(painter, rect, Qt::AlignCenter);
}

void TextMark::paintAnnotation(QPainter &painter,
                               const QRect &eventRect,
                               QRectF *annotationRect,
                               const qreal fadeInOffset,
                               const qreal fadeOutOffset,
                               const QPointF &contentOffset) const
{
    QString text = lineAnnotation();
    if (text.isEmpty())
        return;

    const AnnotationRects &rects = annotationRects(*annotationRect,
                                                   painter.fontMetrics(),
                                                   fadeInOffset,
                                                   fadeOutOffset);
    annotationRect->setRight(rects.fadeOutRect.right());
    const QRectF eventRectF(eventRect);
    if (!(rects.fadeInRect.intersects(eventRectF) || rects.annotationRect.intersects(eventRectF)
          || rects.fadeOutRect.intersects(eventRectF))) {
        return;
    }

    const QColor &markColor = m_color.has_value()
                                  ? Utils::creatorTheme()->color(m_color.value()).toHsl()
                                  : painter.pen().color();

    const FontSettings &fontSettings = m_baseTextDocument->fontSettings();
    const AnnotationColors &colors = AnnotationColors::getAnnotationColors(
        markColor, fontSettings.toTextCharFormat(C_TEXT).background().color());

    painter.save();
    QLinearGradient grad(rects.fadeInRect.topLeft() - contentOffset,
                         rects.fadeInRect.topRight() - contentOffset);
    grad.setColorAt(0.0, Qt::transparent);
    grad.setColorAt(1.0, colors.rectColor);
    painter.fillRect(rects.fadeInRect, grad);
    painter.fillRect(rects.annotationRect, colors.rectColor);
    painter.setPen(colors.textColor);
    paintIcon(&painter, rects.iconRect.toAlignedRect());
    painter.drawText(rects.textRect, Qt::AlignLeft, rects.text);
    if (rects.fadeOutRect.isValid()) {
        grad = QLinearGradient(rects.fadeOutRect.topLeft() - contentOffset,
                               rects.fadeOutRect.topRight() - contentOffset);
        grad.setColorAt(0.0, colors.rectColor);
        grad.setColorAt(1.0, Qt::transparent);
        painter.fillRect(rects.fadeOutRect, grad);
    }
    painter.restore();
}

TextMark::AnnotationRects TextMark::annotationRects(const QRectF &boundingRect,
                                                    const QFontMetrics &fm,
                                                    const qreal fadeInOffset,
                                                    const qreal fadeOutOffset) const
{
    AnnotationRects rects;
    rects.text = lineAnnotation();
    if (rects.text.isEmpty())
        return rects;
    rects.fadeInRect = boundingRect;
    rects.fadeInRect.setWidth(fadeInOffset);
    rects.annotationRect = boundingRect;
    rects.annotationRect.setLeft(rects.fadeInRect.right());
    const bool drawIcon = !icon().isNull();
    constexpr qreal margin = 1;
    rects.iconRect = QRectF(rects.annotationRect.left(), boundingRect.top(),
                            0, boundingRect.height());
    if (drawIcon)
        rects.iconRect.setWidth(rects.iconRect.height() * m_widthFactor);
    rects.textRect = QRectF(rects.iconRect.right() + margin, boundingRect.top(),
                            qreal(fm.horizontalAdvance(rects.text)), boundingRect.height());
    rects.annotationRect.setRight(rects.textRect.right() + margin);
    if (rects.annotationRect.right() > boundingRect.right()) {
        rects.textRect.setRight(boundingRect.right() - margin);
        rects.text = fm.elidedText(rects.text, Qt::ElideRight, int(rects.textRect.width()));
        rects.annotationRect.setRight(boundingRect.right());
        rects.fadeOutRect = QRectF(rects.annotationRect.topRight(),
                                   rects.annotationRect.bottomRight());
    } else {
        rects.fadeOutRect = boundingRect;
        rects.fadeOutRect.setLeft(rects.annotationRect.right());
        rects.fadeOutRect.setWidth(fadeOutOffset);
    }
    return rects;
}

void TextMark::updateLineNumber(int lineNumber)
{
    m_lineNumber = lineNumber;
}

void TextMark::move(int line)
{
    if (line == m_lineNumber)
        return;
    const int previousLine = m_lineNumber;
    m_lineNumber = line;
    if (m_baseTextDocument)
        m_baseTextDocument->moveMark(this, previousLine);
}

void TextMark::updateBlock(const QTextBlock &)
{}

void TextMark::removedFromEditor()
{}

void TextMark::updateMarker()
{
    if (m_baseTextDocument)
        m_baseTextDocument->updateMark(this);
}

void TextMark::setPriority(TextMark::Priority prioriy)
{
    m_priority = prioriy;
    updateMarker();
}

bool TextMark::isVisible() const
{
    return m_visible;
}

void TextMark::setVisible(bool visible)
{
    m_visible = visible;
    updateMarker();
}

double TextMark::widthFactor() const
{
    return m_widthFactor;
}

void TextMark::setWidthFactor(double factor)
{
    m_widthFactor = factor;
}

bool TextMark::isClickable() const
{
    return false;
}

void TextMark::clicked()
{}

bool TextMark::isDraggable() const
{
    return false;
}

void TextMark::dragToLine(int lineNumber)
{
    Q_UNUSED(lineNumber)
}

void TextMark::addToToolTipLayout(QGridLayout *target) const
{
    auto contentLayout = new QVBoxLayout;
    addToolTipContent(contentLayout);
    if (contentLayout->count() <= 0)
        return;

    // Left column: text mark icon
    const int row = target->rowCount();
    const QIcon icon = this->icon();
    if (!icon.isNull()) {
        auto iconLabel = new QLabel;
        iconLabel->setPixmap(icon.pixmap(16, 16));
        target->addWidget(iconLabel, row, 0, Qt::AlignTop | Qt::AlignHCenter);
    }

    // Middle column: tooltip content
    target->addLayout(contentLayout, row, 1);

    // Right column: action icons/button
    QVector<QAction *> actions = m_actions;
    if (m_settingsAction)
        actions << m_settingsAction;
    if (!actions.isEmpty()) {
        auto actionsLayout = new QHBoxLayout;
        QMargins margins = actionsLayout->contentsMargins();
        margins.setLeft(margins.left() + 5);
        actionsLayout->setContentsMargins(margins);
        for (QAction *action : qAsConst(actions)) {
            QTC_ASSERT(!action->icon().isNull(), continue);
            auto button = new QToolButton;
            button->setIcon(action->icon());
            button->setToolTip(action->toolTip());
            QObject::connect(button, &QToolButton::clicked, action, &QAction::triggered);
            QObject::connect(button, &QToolButton::clicked, []() {
                Utils::ToolTip::hideImmediately();
            });
            actionsLayout->addWidget(button, 0, Qt::AlignTop | Qt::AlignRight);
        }
        target->addLayout(actionsLayout, row, 2);
    }
}

bool TextMark::addToolTipContent(QLayout *target) const
{
    bool useDefaultToolTip = false;
    QString text = toolTip();
    if (text.isEmpty()) {
        useDefaultToolTip = true;
        text = m_defaultToolTip;
        if (text.isEmpty())
            return false;
    }

    auto textLabel = new QLabel;
    textLabel->setOpenExternalLinks(true);
    textLabel->setText(text);
    // Differentiate between tool tips that where explicitly set and default tool tips.
    textLabel->setDisabled(useDefaultToolTip);
    target->addWidget(textLabel);

    return true;
}

void TextMark::setIcon(const QIcon &icon)
{
    m_icon = icon;
    m_iconProvider = std::function<QIcon()>();
    updateMarker();
}

void TextMark::setIconProvider(const std::function<QIcon ()> &iconProvider)
{
    m_iconProvider = iconProvider;
    updateMarker();
}

const QIcon TextMark::icon() const
{
    return m_iconProvider ? m_iconProvider() : m_icon;
}

Utils::optional<Theme::Color> TextMark::color() const
{
    return m_color;
}

void TextMark::setColor(const Theme::Color &color)
{
    m_color = color;
    updateMarker();
}

void TextMark::setLineAnnotation(const QString &lineAnnotation)
{
    m_lineAnnotation = lineAnnotation;
    updateMarker();
}

void TextMark::setToolTipProvider(const std::function<QString()> &toolTipProvider)
{
    m_toolTipProvider = toolTipProvider;
}

QString TextMark::toolTip() const
{
    return m_toolTipProvider ? m_toolTipProvider() : m_toolTip;
}

void TextMark::setToolTip(const QString &toolTip)
{
    m_toolTip = toolTip;
    m_toolTipProvider = std::function<QString()>();
}

QVector<QAction *> TextMark::actions() const
{
    return m_actions;
}

void TextMark::setActions(const QVector<QAction *> &actions)
{
    m_actions = actions;
}

void TextMark::setSettingsPage(Id settingsPage)
{
    delete m_settingsAction;
    m_settingsAction = new QAction;
    m_settingsAction->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    m_settingsAction->setToolTip(tr("Show Diagnostic Settings"));
    QObject::connect(m_settingsAction, &QAction::triggered, Core::ICore::instance(),
        [settingsPage] { Core::ICore::showOptionsDialog(settingsPage); },
        Qt::QueuedConnection);
}

TextMarkRegistry::TextMarkRegistry(QObject *parent)
    : QObject(parent)
{
    connect(EditorManager::instance(), &EditorManager::editorOpened,
            this, &TextMarkRegistry::editorOpened);

    connect(DocumentManager::instance(), &DocumentManager::allDocumentsRenamed,
            this, &TextMarkRegistry::allDocumentsRenamed);
    connect(DocumentManager::instance(), &DocumentManager::documentRenamed,
            this, &TextMarkRegistry::documentRenamed);
}

void TextMarkRegistry::add(TextMark *mark)
{
    instance()->m_marks[mark->fileName()].insert(mark);
    if (TextDocument *document = TextDocument::textDocumentForFilePath(mark->fileName()))
        document->addMark(mark);
}

bool TextMarkRegistry::remove(TextMark *mark)
{
    return instance()->m_marks[mark->fileName()].remove(mark);
}

TextMarkRegistry *TextMarkRegistry::instance()
{
    if (!m_instance)
        m_instance = new TextMarkRegistry(TextEditorPlugin::instance());
    return m_instance;
}

void TextMarkRegistry::editorOpened(IEditor *editor)
{
    auto document = qobject_cast<TextDocument *>(editor ? editor->document() : nullptr);
    if (!document)
        return;
    if (!m_marks.contains(document->filePath()))
        return;

    foreach (TextMark *mark, m_marks.value(document->filePath()))
        document->addMark(mark);
}

void TextMarkRegistry::documentRenamed(IDocument *document,
                                       const FilePath &oldPath,
                                       const FilePath &newPath)
{
    auto baseTextDocument = qobject_cast<TextDocument *>(document);
    if (!baseTextDocument)
        return;
    if (!m_marks.contains(oldPath))
        return;

    QSet<TextMark *> toBeMoved;
    foreach (TextMark *mark, baseTextDocument->marks())
        toBeMoved.insert(mark);

    m_marks[oldPath].subtract(toBeMoved);
    m_marks[newPath].unite(toBeMoved);

    foreach (TextMark *mark, toBeMoved)
        mark->updateFileName(newPath);
}

void TextMarkRegistry::allDocumentsRenamed(const FilePath &oldPath, const FilePath &newPath)
{
    if (!m_marks.contains(oldPath))
        return;

    QSet<TextMark *> oldFileNameMarks = m_marks.value(oldPath);

    m_marks[newPath].unite(oldFileNameMarks);
    m_marks[oldPath].clear();

    foreach (TextMark *mark, oldFileNameMarks)
        mark->updateFileName(newPath);
}

QHash<AnnotationColors::SourceColors, AnnotationColors> AnnotationColors::m_colorCache;

AnnotationColors &AnnotationColors::getAnnotationColors(const QColor &markColor,
                                                        const QColor &backgroundColor)
{
    auto highClipHsl = [](qreal value) {
        return std::max(0.7, std::min(0.9, value));
    };
    auto lowClipHsl = [](qreal value) {
        return std::max(0.1, std::min(0.3, value));
    };
    AnnotationColors &colors = m_colorCache[{markColor, backgroundColor}];
    if (!colors.rectColor.isValid() || !colors.textColor.isValid()) {
        const double backgroundLightness = backgroundColor.lightnessF();
        const double foregroundLightness = backgroundLightness > 0.5
                ? lowClipHsl(backgroundLightness - 0.5)
                : highClipHsl(backgroundLightness + 0.5);

        colors.rectColor = markColor;
        colors.rectColor.setAlphaF(0.15f);

        colors.textColor.setHslF(markColor.hslHueF(),
                                 markColor.hslSaturationF(),
                                 foregroundLightness);
    }
    return colors;
}

} // namespace TextEditor

#include "textmark.moc"
