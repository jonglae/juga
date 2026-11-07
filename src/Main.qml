import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    visible: true
    width: 900
    height: 820
    minimumWidth: 690
    minimumHeight: 700
    title: qsTr("Juga · Korean stocks")
    color: "#111114"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property bool helpVisible: false
    property string orderSide: "BUY"
    property string orderType: "LIMIT"
    property bool highValueChecked: false
    property string pendingSide: "BUY"

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.refreshPrice() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Korean stocks"); font.pixelSize: 27; font.bold: true; Layout.fillWidth: true }
            Button { text: qsTr("Settings"); onClicked: settingsDialog.open() }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: symbolField
                Layout.fillWidth: true
                text: backend.symbol
                placeholderText: qsTr("6 digit stock code · e.g. 005930")
                maximumLength: 6
                inputMethodHints: Qt.ImhUppercaseOnly
                onAccepted: backend.watchSymbol(text)
            }
            Button { text: qsTr("Watch"); highlighted: true; onClicked: backend.watchSymbol(symbolField.text) }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            radius: theme.radius
            color: "#202025"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 7
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: backend.stockName.length ? backend.stockName : qsTr("Stock"); font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
                    Label { text: backend.symbol; opacity: 0.6 }
                }
                Label {
                    text: backend.price.length ? Number(backend.price).toLocaleString(Qt.locale(), 'f', 0) + qsTr(" KRW") : "—"
                    font.pixelSize: 42; font.bold: true
                }
                Label { text: backend.tradeTime.length ? qsTr("Last trade · %1").arg(backend.tradeTime) : qsTr("Waiting for a trade update"); opacity: 0.72 }
                Item { Layout.fillHeight: true }
                Label { text: backend.status; wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.8 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Trading"); font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
            ComboBox {
                id: accountBox
                Layout.preferredWidth: 230
                model: backend.accounts
                textRole: "label"
                valueRole: "seq"
                currentIndex: {
                    for (let i = 0; i < count; ++i)
                        if (model[i].seq === backend.selectedAccount) return i
                    return count > 0 ? 0 : -1
                }
                onActivated: backend.selectedAccount = currentValue
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ButtonGroup { id: sideGroup }
            RadioButton { text: qsTr("Buy"); checked: true; ButtonGroup.group: sideGroup; onToggled: if (checked) win.orderSide = "BUY" }
            RadioButton { text: qsTr("Sell"); ButtonGroup.group: sideGroup; onToggled: if (checked) win.orderSide = "SELL" }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: typeBox
                model: [qsTr("Limit order"), qsTr("Market order")]
                currentIndex: 0
                onActivated: win.orderType = currentIndex === 0 ? "LIMIT" : "MARKET"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField { id: quantityField; Layout.fillWidth: true; placeholderText: qsTr("Quantity"); inputMethodHints: Qt.ImhDigitsOnly }
            TextField {
                id: priceField
                Layout.fillWidth: true
                placeholderText: qsTr("Limit price (KRW)")
                inputMethodHints: Qt.ImhDigitsOnly
                text: backend.price
                enabled: win.orderType === "LIMIT"
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            highlighted: true
            enabled: backend.configured && accountBox.count > 0 && !backend.submitting
            text: win.orderSide === "BUY" ? qsTr("Review buy order") : qsTr("Review sell order")
            onClicked: {
                win.pendingSide = win.orderSide
                win.highValueChecked = false
                confirmDialog.open()
            }
        }
        Label { text: backend.orderStatus; wrapMode: Text.Wrap; Layout.fillWidth: true; visible: backend.orderStatus.length > 0 }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.58
            text: qsTr("Market orders may fill at a different price. A submitted order can remain open or be rejected; check its status in Toss Securities.")
        }
        Item { Layout.fillHeight: true }
        Label { text: win.helpVisible ? qsTr("Space: refresh · ?: shortcuts · Q: quit") : qsTr("Press ? for shortcuts · Price feed by Toss Securities Open API"); opacity: 0.5 }
    }

    Dialog {
        id: settingsDialog
        title: qsTr("Toss API settings")
        modal: true
        anchors.centerIn: parent
        width: Math.min(510, win.width - 40)
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: backend.saveCredentials(clientIdField.text, clientSecretField.text)
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: qsTr("Create an API key in Toss Securities WTS and add this computer's public IP to the allowed IP list."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField { id: clientIdField; Layout.fillWidth: true; placeholderText: qsTr("Client ID"); text: "" }
            TextField { id: clientSecretField; Layout.fillWidth: true; placeholderText: qsTr("Client secret"); echoMode: TextInput.Password; text: "" }
            Label { text: qsTr("Credentials are stored in ~/.config/juga/settings.ini with owner-only file permissions."); wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.65 }
        }
    }

    Dialog {
        id: confirmDialog
        title: qsTr("Confirm stock order")
        modal: true
        anchors.centerIn: parent
        width: Math.min(500, win.width - 40)
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("%1 %2 shares of %3 (%4) as a %5 order.")
                    .arg(win.pendingSide === "BUY" ? qsTr("Buy") : qsTr("Sell"))
                    .arg(quantityField.text).arg(backend.stockName).arg(backend.symbol)
                    .arg(win.orderType === "LIMIT" ? qsTr("limit") : qsTr("market"))
            }
            Label {
                Layout.fillWidth: true
                text: win.orderType === "LIMIT"
                    ? qsTr("Limit price: ₩%1 · Estimated total: ₩%2").arg(Number(priceField.text).toLocaleString(Qt.locale(), 'f', 0)).arg((Number(quantityField.text) * Number(priceField.text)).toLocaleString(Qt.locale(), 'f', 0))
                    : qsTr("Latest quote: ₩%1 · Estimated total: ₩%2").arg(Number(backend.price).toLocaleString(Qt.locale(), 'f', 0)).arg((Number(quantityField.text) * Number(backend.price)).toLocaleString(Qt.locale(), 'f', 0))
            }
            Label {
                visible: win.orderType === "MARKET"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#ffb4ab"
                text: qsTr("Market orders do not guarantee a price. The final execution price may differ from the displayed quote.")
            }
            CheckBox {
                id: highValueCheck
                visible: Number(quantityField.text) * Number(priceField.text) >= 100000000
                text: qsTr("I confirm this order is at least ₩100 million.")
            }
            Label {
                visible: Number(quantityField.text) * Number(priceField.text) >= 3000000000
                text: qsTr("Orders of ₩3 billion or more cannot be submitted.")
                color: "#ffb4ab"
            }
        }
        footer: DialogButtonBox {
            alignment: Qt.AlignRight
            Button { text: qsTr("Cancel"); onClicked: confirmDialog.reject() }
            Button {
                text: qsTr("Send order")
                highlighted: true
                onClicked: {
                    const amount = Number(quantityField.text) * Number(win.orderType === "LIMIT" ? priceField.text : backend.price)
                    if (amount >= 3000000000) return
                    if (amount >= 100000000 && !highValueCheck.checked) return
                    backend.submitOrder(win.pendingSide, win.orderType, quantityField.text.trim(),
                                        win.orderType === "LIMIT" ? priceField.text.trim() : "",
                                        highValueCheck.checked)
                    confirmDialog.close()
                }
            }
        }
    }
}
