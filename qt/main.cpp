/****************************************************************************
**
** Copyright (C) 2013 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QTextStream>
#include <QCoreApplication>
#include <QStringList>

#include "serialpropellerconnection.h"
#include "propellerloader.h"

QT_USE_NAMESPACE

int main(int argc, char *argv[])
{
    QCoreApplication coreApplication(argc, argv);
    int argumentCount = QCoreApplication::arguments().size();
    QStringList argumentList = QCoreApplication::arguments();

    QTextStream standardOutput(stdout);

    if (argumentCount != 3) {
        standardOutput << QObject::tr("Usage: %1 <port> <file>").arg(argumentList.first()) << endl;
        return 1;
    }

    QByteArray port(argumentList.at(1).toLocal8Bit());
    QByteArray file(argumentList.at(2).toLocal8Bit());

    SerialPropellerConnection connection;
    if (connection.open(port.data(), 115200) != 0) {
        printf("Opening %s failed\n", port.data());
        return 1;
    }

    PropellerLoader loader(connection);
    if (loader.load(file.data(), ltDownloadAndRun) != 0) {
        printf("Loading %s failed\n", file.data());
        return 1;
    }

    return 0;
}
