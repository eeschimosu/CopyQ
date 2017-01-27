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

#include "tabicons.h"

#include "common/appconfig.h"
#include "common/common.h"
#include "common/config.h"
#include "common/settings.h"
#include "gui/iconfactory.h"

#include <QComboBox>
#include <QDir>
#include <QHash>
#include <QIcon>

namespace {

QHash<QString, QString> tabIcons()
{
    QHash<QString, QString> icons;

    Settings settings;
    const int size = settings.beginReadArray("Tabs");
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        icons.insert(settings.value("name").toString(),
                     settings.value("icon").toString());
    }

    return icons;
}

} // namespace

QStringList tabs()
{
    QStringList tabs = AppConfig().option<Config::tabs>();
    tabs.removeAll(QString());
    return tabs;
}

void setTabs(const QStringList &tabs)
{
    AppConfig().setOption("tabs", tabs);
}

QStringList savedTabs()
{
    QStringList tabs = ::tabs();

    const QString configPath = settingsDirectoryPath();

    QStringList files = QDir(configPath).entryList(QStringList("*_tab_*.dat"));
    files.append( QDir(configPath).entryList(QStringList("*_tab_*.dat.tmp")) );

    QRegExp re("_tab_([^.]*)");

    foreach (const QString fileName, files) {
        if ( fileName.contains(re) ) {
            const QString tabName =
                    getTextData(QByteArray::fromBase64(re.cap(1).toUtf8()));
            if ( !tabName.isEmpty() && !tabs.contains(tabName) )
                tabs.append(tabName);
        }
    }

    return tabs;
}

QString getIconNameForTabName(const QString &tabName)
{
    Settings settings;
    const int size = settings.beginReadArray("Tabs");
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        if (settings.value("name").toString() == tabName)
            return settings.value("icon").toString();
    }

    return QString();
}

void setIconNameForTabName(const QString &name, const QString &icon)
{
    QHash<QString, QString> icons = tabIcons();
    icons[name] = icon;

    Settings settings;
    settings.beginWriteArray("Tabs");
    int i = 0;

    for ( const auto &tabName : icons.keys() ) {
        settings.setArrayIndex(i++);
        settings.setValue("name", tabName);
        settings.setValue("icon", icons[tabName]);
    }

    settings.endArray();
}

QIcon getIconForTabName(const QString &tabName)
{
    const QString fileName = getIconNameForTabName(tabName);
    return fileName.isEmpty() ? QIcon() : iconFromFile(fileName);
}

void initTabComboBox(QComboBox *comboBox)
{
    setComboBoxItems(comboBox, tabs());

    for (int i = 1; i < comboBox->count(); ++i) {
        const QString tabName = comboBox->itemText(i);
        const QIcon icon = getIconForTabName(tabName);
        comboBox->setItemIcon(i, icon);
    }
}

void setDefaultTabItemCounterStyle(QWidget *widget)
{
    QFont font = widget->font();
    const qreal pointSize = font.pointSizeF();
    if (pointSize > 0.0)
        font.setPointSizeF(pointSize * 0.7);
    else
        font.setPixelSize(font.pixelSize() * 0.7);
    widget->setFont(font);

    QPalette pal = widget->palette();
    const QPalette::ColorRole role = widget->foregroundRole();
    QColor color = pal.color(role);
    color.setAlpha( qMax(50, color.alpha() - 100) );
    color.setRed( qMin(255, color.red() + 120) );
    pal.setColor(role, color);
    widget->setPalette(pal);
}

void setComboBoxItems(QComboBox *comboBox, const QStringList &items)
{
    const QString text = comboBox->currentText();
    comboBox->clear();
    comboBox->addItem(QString());
    comboBox->addItems(items);
    comboBox->setEditText(text);

    const int currentIndex = comboBox->findText(text);
    if (currentIndex != -1)
        comboBox->setCurrentIndex(currentIndex);
}
