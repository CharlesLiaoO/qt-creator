import QtQuick 1.0

ListView {
    width: 160
    height: 160
    model:  ListModel {
        ListElement {
            name: "Grey"
            colorCode: "grey"
        }
        ListElement {
            name: "Red"
            colorCode: "red"
        }
        ListElement {
            name: "Blue"
            colorCode: "blue"
        }
        ListElement {
            name: "Green"
            colorCode: "green"
        }
    }

    delegate:  Item {
        height: 40
        x: 5
        Row {
            id: row1
            spacing: 10
            Rectangle { width: 40; height: 40; color: colorCode; }
            Text {
                text: name
                anchors.verticalCenter: parent.verticalCenter
                font.bold: true
            }
        }
    }
}
