// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


import QtQuick 2.0


Rectangle {
    id: root
    color: "black"
    anchors.fill: parent

    property real progress: 0
    property bool showDataProgressText: true
    property alias stage: gameCounter.text

    Behavior on progress { NumberAnimation {} }

/*
    Image {
        id: logo
        source: "assets/vc_logo.png"
        width: Math.min(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        verticalAlignment: Image.AlignBottom

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.verticalCenter
        anchors.bottomMargin: vpx(-45)
    }*/

    Rectangle {
        id: progressRoot

        property int padding: 0

        //width: logo.width * 0.94
        height: 21
        radius: 10
        color: "#3f3f3f"

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 200
        anchors.bottomMargin: 100
        anchors.horizontalCenter: parent.horizontalCenter
        //clip: true

        Image {
            source: "assets/vcload_center.png"
            //opacity: 0.5
            horizontalAlignment: Image.AlignLeft

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: loadleft.right
            anchors.right: parent.right
            anchors.rightMargin: Math.min(((parent.width) * (1.0 - root.progress)), (parent.width-60))



            Image {
                source: "assets/vcload_right.png"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.right
                //anchors.right: parent.right
                fillMode: Image.PreserveAspectFit
            }
        }

        Image {
            id: loadleft
            source: "assets/vcload_left.png"
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            fillMode: Image.PreserveAspectFit
        }

    }

    Text {
        id: gameCounter
        visible: showDataProgressText

        color: "#999"
        font.pixelSize: vpx(16)
        font.family: global.fonts.sans
        font.italic: true

        anchors.top: progressRoot.bottom
        anchors.topMargin: vpx(8)
        anchors.right: progressRoot.right
        anchors.rightMargin: vpx(5)
    }
}
