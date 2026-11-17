// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PS5 pairing form: an Account ID and host typed and then 配对并保存 clicked straight away (the button
// takes no focus, so no editingFinished) must reach ps5Pair and stay in the fields afterwards.
import QtQuick
import QtTest
import Veyra
import VeyraTest 1.0

Item {
    id: fixture
    width: 900; height: 1500
    QtObject {
        id: backend
        property var ps5: ({host:"",account:"",profile:"",busy:false,active:false,quality:3,codec:0,bitrate:2,decode:0,sampling:1,status:""})
        property var ps5Profiles: []
        property var ps5Control: ({available:false,enabled:false})
        property string pairedAccount: ""
        property string pairedHost: ""
        property string pairedPin: ""
        function ps5Load() {}
        function ps5OpenAccountChecker() {}
        function ps5Set(key,value) { const next=Object.assign({},ps5);next[key]=String(value).trim();ps5=next;return true }
        // As the bridge: reads the form, then a status refresh re-emits the form.
        function ps5Pair(pin) {
            pairedAccount=ps5.account; pairedHost=ps5.host; pairedPin=pin
            const next=Object.assign({},ps5); next.status="paired"; ps5=next
        }
    }
    Item {
        id: host
        anchors.fill: parent
        property string dialog: "ps5"
        property real motionScale: 1
        function close() {}
        function open(key) { dialog=key }
    }
    Ps5SourceDialog { id: panel; property var veyra: backend; dialogHost: host }
    TestCase {
        name: "PushPs5Pairing"
        when: windowShown
        function typeInto(field, value) {
            const input = field.children[0]
            mouseClick(field)
            for (const ch of value) keyClick(ch)
            verify(input.text === value, input.text)
        }
        function test_typed_values_reach_pairing_and_stay() {
            const account = findChild(panel, "ps5-account-id")
            const hostField = findChild(panel, "ps5-host")
            verify(account && hostField)
            typeInto(hostField, "192.168.1.20")
            typeInto(account, "1234567890")
            compare(backend.ps5.host, "192.168.1.20")
            compare(backend.ps5.account, "1234567890")
            // 配对并保存 takes no focus; the status refresh after it must not wipe the fields.
            typeInto(findChild(panel, "ps5-pair").parent.children[0], "12345678")
            mouseClick(findChild(panel, "ps5-pair"))
            compare(backend.pairedPin, "12345678")
            compare(backend.pairedAccount, "1234567890")
            compare(backend.pairedHost, "192.168.1.20")
            compare(account.text, "1234567890")
            compare(hostField.text, "192.168.1.20")
        }
    }
}
