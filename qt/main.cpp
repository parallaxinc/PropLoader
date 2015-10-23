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
#include <QCommandLineParser>
#include <QStringList>

#include "serialpropellerconnection.h"
#include "xbeepropellerconnection.h"
#include "propellerloader.h"
#include "fastpropellerloader.h"

QT_USE_NAMESPACE

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString port, file;

    QCommandLineParser parser;

    parser.addPositionalArgument("file", QCoreApplication::translate("main", "File to load."));

    QCommandLineOption portOption(QStringList() << "p" << "port",
            QCoreApplication::translate("main", "Serial port."),
            QCoreApplication::translate("main", "port"));
    parser.addOption(portOption);

    parser.process(app);

#if 0
    if (parser.isSet(portOption))
        port = parser.value(portOption);
    else {
        QStringList ports(SerialPropellerConnection::availablePorts("/dev/cu.usbserial"));
        if (ports.size() < 1) {
            printf("No ports found\n");
            return 1;
        }
        port = ports[0];
    }
#endif

    if (parser.positionalArguments().size() != 1)
        parser.showHelp();
    file = parser.positionalArguments()[0];

#if 0
    printf("Opening %s\n", port.toLatin1().data());
    SerialPropellerConnection connection;
    if (connection.open(port.toLatin1().data(), 115200) != 0) {
        printf("Opening %s failed\n", port.toLatin1().data());
        return 1;
    }
#endif

    XbeePropellerConnection xbeeConnection("10.0.1.88");

    printf("Loading %s\n", file.toLatin1().data());

    PropellerImage image;
    if (image.load(file.toLatin1().data()) != 0) {
        printf("error: reading '%s'\n", file.toLatin1().data());
        return 1;
    }

    //FastPropellerLoader loader(connection);
    FastPropellerLoader loader(xbeeConnection);
    if (loader.load(image, ltDownloadAndRun) != 0) {
        printf("error: loading '%s'\n", file.toLatin1().data());
        return 1;
    }

    return 0;
}
