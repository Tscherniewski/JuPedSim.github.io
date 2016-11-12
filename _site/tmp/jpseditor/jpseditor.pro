 # This file is part of JuPedSim.
 #
 # JuPedSim is free software: you can redistribute it and/or modify
 # it under the terms of the GNU Lesser General Public License as published by
 # the Free Software Foundation, either version 3 of the License, or
 # any later version.
 #
 # JuPedSim is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU Lesser General Public License
 # along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 #
 # copyright   <2009-2014> Forschungszentrum Juelich GmbH.
 
 # Compatibility with Qt4 and Qt5
greaterThan(QT_MAJOR_VERSION, 4):QT += widgets

TEMPLATE = app
TARGET = JuPedSim
CONFIG += qt xml gui;
CONFIG += c++11
QMAKE_CXXFLAGS += -std=c++11

FORMS += \
    forms/mainwindow.ui \
    forms/roomwidget.ui \
    forms/widgetlandmark.ui \
    forms/widgetsettings.ui

HEADERS += \
    src/mainWindow.h \
    src/GraphicView.h \
    src/roomwidget.h \
    src/rooms.h \
    src/datamanager.h \
    src/jpscrossing.h \
    src/jpsLineItem.h \
    src/jpsexit.h \
    src/jpsobstacle.h \
    src/jpslandmark.h \
    dxflib/src/dl_writer_ascii.h \
    dxflib/src/dl_writer.h \
    dxflib/src/dl_global.h \
    dxflib/src/dl_extrusion.h \
    dxflib/src/dl_exception.h \
    dxflib/src/dl_entities.h \
    dxflib/src/dl_dxf.h \
    dxflib/src/dl_creationinterface.h \
    dxflib/src/dl_creationadapter.h \
    dxflib/src/dl_codes.h \
    dxflib/src/dl_attributes.h \
    src/jpswaypoint.h \
    src/widgetlandmark.h \
    src/graphicscene.h \
    src/widgetsettings.h \
    src/jpsyahpointer.h \
    src/jpsconnection.h \
    src/UndoFramework/actionstack.h \
    src/UndoFramework/action.h \
    src/UndoFramework/lineaction.h \
    src/jpsregion.h


SOURCES += \
    src/mainWindow.cpp \
    src/main.cpp \
    src/GraphicView.cpp \
    src/roomwidget.cpp \
    src/rooms.cpp \
    src/datamanager.cpp \
    src/jpscrossing.cpp \
    src/jpsLineItem.cpp \
    src/jpsexit.cpp \
    src/jpsobstacle.cpp \
    src/jpslandmark.cpp \
    dxflib/src/dl_writer_ascii.cpp \
    dxflib/src/dl_dxf.cpp \
    src/jpswaypoint.cpp \
    src/widgetlandmark.cpp \
    src/graphicscene.cpp \
    src/widgetsettings.cpp \
    src/jpsyahpointer.cpp \
    src/jpsconnection.cpp \
    src/UndoFramework/actionstack.cpp \
    src/UndoFramework/action.cpp \
    src/UndoFramework/lineaction.cpp \
    src/jpsregion.cpp


RESOURCES +=  forms/Ressource.qrc

RC_FILE = icons/jpseditor.rc
#osx fix
macx:ICON = icons/jupedsim.icns



