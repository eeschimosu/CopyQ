/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "itemwidget.h"

#include "common/command.h"
#include "common/contenttype.h"
#include "item/itemeditor.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QMimeData>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPalette>
#include <QTextEdit>
#include <QTextFormat>
#include <QWidget>

#include <QDebug>

namespace {

bool canMouseInteract(const QMouseEvent &event)
{
    return event.modifiers() & Qt::ShiftModifier;
}

bool containsRichText(const QTextDocument &document)
{
    return document.allFormats().size() > 3;
}

QString findImageFormat(const QMimeData &data)
{
    static const QStringList imageFormats = QStringList()
            << QString("image/svg+xml")
            << QString("image/png")
            << QString("image/bmp")
            << QString("image/jpeg")
            << QString("image/gif");

    for (const auto &format : imageFormats) {
        if ( data.hasFormat(format) )
            return format;
    }

    return QString();
}

/**
 * Text edit with support for pasting/dropping images.
 *
 * Images are saved in HTML in base64-encoded form.
 */
class TextEdit : public QTextEdit {
public:
    explicit TextEdit(QWidget *parent) : QTextEdit(parent) {}

protected:
    bool canInsertFromMimeData(const QMimeData *source) const
    {
        return source->hasImage() || QTextEdit::canInsertFromMimeData(source);
    }

    bool canPaste() const
    {
        const QMimeData *source = QApplication::clipboard()->mimeData();
        return source->hasImage() || QTextEdit::canPaste();
    }

    void insertFromMimeData(const QMimeData *source)
    {
        const QString mime = findImageFormat(*source);

        if (!mime.isEmpty()) {
            const QByteArray imageData = source->data(mime);
            textCursor().insertHtml(
                        "<img src=\"data:" + mime + ";base64," + imageData.toBase64() + "\" />");
        } else {
            QTextEdit::insertFromMimeData(source);
        }
    }
};

} // namespace

ItemWidget::ItemWidget(QWidget *widget)
    : m_re()
    , m_widget(widget)
{
    Q_ASSERT(widget != nullptr);

    // Object name for style sheet.
    widget->setObjectName("item");

    // Item widgets are not focusable.
    widget->setFocusPolicy(Qt::NoFocus);

    // Limit size of items.
    widget->setMaximumSize(2048, 2048);

    // Disable drag'n'drop by default.
    widget->setAcceptDrops(false);
}

void ItemWidget::setHighlight(const QRegExp &re, const QFont &highlightFont,
                              const QPalette &highlightPalette)
{
    QPalette palette( widget()->palette() );
    palette.setColor(QPalette::Highlight, highlightPalette.base().color());
    palette.setColor(QPalette::HighlightedText, highlightPalette.text().color());
    widget()->setPalette(palette);

    if (m_re == re)
        return;
    m_re = re;
    highlight(re, highlightFont, highlightPalette);
}

QWidget *ItemWidget::createEditor(QWidget *parent) const
{
    QTextEdit *editor = new TextEdit(parent);
    editor->setFrameShape(QFrame::NoFrame);
    return editor;
}

void ItemWidget::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QTextEdit *textEdit = qobject_cast<QTextEdit *>(editor);
    if (textEdit != nullptr) {
        if ( index.data(contentType::hasHtml).toBool() ) {
            const QString html = index.data(contentType::html).toString();
            textEdit->setHtml(html);
        } else {
            const QString text = index.data(Qt::EditRole).toString();
            textEdit->setPlainText(text);
        }
        textEdit->selectAll();
    }
}

void ItemWidget::setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const
{
    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (textEdit != nullptr) {
        // Clear text.
        model->setData(index, QString());

        QVariantMap data;
        data["text/plain"] = textEdit->toPlainText().toUtf8();
        if ( containsRichText(*textEdit->document()) )
            data["text/html"] = textEdit->toHtml().toUtf8();
        model->setData(index, data, contentType::updateData);

        textEdit->document()->setModified(false);
    }
}

bool ItemWidget::hasChanges(QWidget *editor) const
{
    QTextEdit *textEdit = (qobject_cast<QTextEdit *>(editor));
    return textEdit != nullptr && textEdit->document() && textEdit->document()->isModified();
}

QObject *ItemWidget::createExternalEditor(const QModelIndex &, QWidget *) const
{
    return nullptr;
}

void ItemWidget::updateSize(const QSize &maximumSize, int idealWidth)
{
    QWidget *w = widget();
    w->setMaximumSize(maximumSize);
    const int idealHeight = w->heightForWidth(idealWidth);
    const int maximumHeight = w->heightForWidth(maximumSize.width());
    if (idealHeight <= 0 && maximumHeight <= 0)
        w->resize(w->sizeHint());
    else if (idealHeight != maximumHeight)
        w->setFixedSize( maximumSize.width(), maximumHeight );
    else
        w->setFixedSize(idealWidth, idealHeight);
}

void ItemWidget::setCurrent(bool current)
{
    // Propagate mouse events to item list until the item is selected.
    widget()->setAttribute(Qt::WA_TransparentForMouseEvents, !current);
}

bool ItemWidget::filterMouseEvents(QTextEdit *edit, QEvent *event)
{
    QEvent::Type type = event->type();

    switch (type) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( !canMouseInteract(*e) )
            e->ignore();
        else if (e->button() == Qt::LeftButton)
            edit->setTextCursor( edit->cursorForPosition(e->pos()) );

        break;
    }

    case QEvent::MouseMove: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( !canMouseInteract(*e) )
            e->ignore();

        break;
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( canMouseInteract(*e) && edit->textCursor().hasSelection() )
            edit->copy();

        e->ignore();

        break;
    }

    default:
        return false;
    }

    Qt::TextInteractionFlags flags = edit->textInteractionFlags();
    if (event->isAccepted())
        flags |= Qt::TextSelectableByMouse;
    else
        flags &= ~Qt::TextSelectableByMouse;
    edit->setTextInteractionFlags(flags);

    return false;
}

ItemWidget *ItemLoaderInterface::create(const QModelIndex &, QWidget *) const
{
    return nullptr;
}

bool ItemLoaderInterface::canLoadItems(QFile *) const
{
    return false;
}

bool ItemLoaderInterface::canSaveItems(const QAbstractItemModel &) const
{
    return false;
}

bool ItemLoaderInterface::loadItems(QAbstractItemModel *, QFile *)
{
    return false;
}

bool ItemLoaderInterface::saveItems(const QAbstractItemModel &, QFile *)
{
    return false;
}

bool ItemLoaderInterface::initializeTab(QAbstractItemModel *)
{
    return false;
}

void ItemLoaderInterface::uninitializeTab(QAbstractItemModel *)
{
}

ItemWidget *ItemLoaderInterface::transform(ItemWidget *, const QModelIndex &)
{
    return nullptr;
}

bool ItemLoaderInterface::canRemoveItems(const QList<QModelIndex> &)
{
    return true;
}

bool ItemLoaderInterface::canMoveItems(const QList<QModelIndex> &)
{
    return true;
}

void ItemLoaderInterface::itemsRemovedByUser(const QList<QModelIndex> &)
{
}

QVariantMap ItemLoaderInterface::copyItem(const QAbstractItemModel &, const QVariantMap &itemData)
{
    return itemData;
}

bool ItemLoaderInterface::matches(const QModelIndex &, const QRegExp &) const
{
    return false;
}

QObject *ItemLoaderInterface::tests(const TestInterfacePtr &) const
{
    return nullptr;
}

const QObject *ItemLoaderInterface::signaler() const
{
    return nullptr;
}

QString ItemLoaderInterface::script() const
{
    return QString();
}

QList<Command> ItemLoaderInterface::commands() const
{
    return QList<Command>();
}
