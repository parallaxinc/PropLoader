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

enum ConnectionType {
    NoConnection,
    SerialConnection,
    XbeeConnection
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString port, ipaddr, file;
    ConnectionType connectionType = NoConnection;

    QCommandLineParser parser;

    parser.addPositionalArgument("file", QCoreApplication::translate("main", "File to load."));

    QCommandLineOption portOption(QStringList() << "p" << "port",
            QCoreApplication::translate("main", "Serial port."),
            QCoreApplication::translate("main", "port"));
    parser.addOption(portOption);

    QCommandLineOption ipOption(QStringList() << "i" << "ip",
            QCoreApplication::translate("main", "IP address."),
            QCoreApplication::translate("main", "ip"));
    parser.addOption(ipOption);

    QCommandLineOption serialOption(QStringList() << "s" << "serial",
            QCoreApplication::translate("main", "Serial download."));
    parser.addOption(serialOption);

    parser.process(app);

    if (parser.isSet(portOption)) {
        port = parser.value(portOption);
        connectionType = SerialConnection;
    }
    else if (parser.isSet(ipOption)) {
        ipaddr = parser.value(ipOption);
        connectionType = XbeeConnection;
    }
    else {
        if (parser.isSet(serialOption)) {
            QStringList ports(SerialPropellerConnection::availablePorts("/dev/cu.usbserial"));
            if (ports.size() < 1) {
                printf("No ports found\n");
                return 1;
            }
            port = ports[0];
            connectionType = SerialConnection;
        }
        else {
            QList<XbeeInfo> modules;
            XbeePropellerConnection::availableModules(modules, 2000);
            if (modules.isEmpty()) {
                printf("No Xbee Wi-Fi modules found.\n");
                return 1;
            }
            printf("Xbee Wi-Fi modules:\n");
            for (QList<XbeeInfo>::iterator i = modules.begin(); i != modules.end(); ++i) {
                XbeeInfo info = *i;
                QHostAddress ipAddr(info.ipAddr);
                printf("ipAddr:          %s\n", ipAddr.toString().toLatin1().data());
                printf("macAddrHigh:     %08x\n", info.macAddrHigh);
                printf("macAddrLow:      %08x\n", info.macAddrLow);
                printf("xbeePort:        %08x\n", info.xbeePort);
                printf("firmwareVersion: %08x\n", info.firmwareVersion);
                printf("cfgChecksum:     %08x\n", info.cfgChecksum);
                printf("nodeId:          '%s'\n", info.nodeID.toLatin1().data());
            }
            ipaddr = QHostAddress(modules[0].ipAddr).toString();
            connectionType = XbeeConnection;
        }
    }

    if (parser.positionalArguments().size() != 1)
        parser.showHelp();
    file = parser.positionalArguments()[0];

    SerialPropellerConnection serialConnection;
    XbeePropellerConnection xbeeConnection;
    PropellerConnection *connection;

    if (connectionType == SerialConnection) {
        printf("Opening %s\n", port.toLatin1().data());
        if (serialConnection.open(port.toLatin1().data(), 115200) != 0) {
            printf("Opening %s failed\n", port.toLatin1().data());
            return 1;
        }
        connection = &serialConnection;
    }

    else if (connectionType == XbeeConnection) {
        printf("Opening %s\n", ipaddr.toLatin1().data());
        if (xbeeConnection.open(ipaddr.toLatin1().data()) != 0) {
            printf("Connection to %s failed\n", ipaddr.toLatin1().data());
            return 1;
        }
        connection = &xbeeConnection;
    }

    else {
        printf("error: no connection type\n");
        return 1;
    }

    printf("Loading %s\n", file.toLatin1().data());

    PropellerImage image;
    if (image.load(file.toLatin1().data()) != 0) {
        printf("error: reading '%s'\n", file.toLatin1().data());
        return 1;
    }

    //FastPropellerLoader loader(connection);
    FastPropellerLoader loader(*connection);
    if (loader.load(image, ltDownloadAndRun) != 0) {
        printf("error: loading '%s'\n", file.toLatin1().data());
        return 1;
    }

    return 0;
}
