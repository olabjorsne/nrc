/**
 * Copyright Tomas Frisberg  Ola Björsne
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

module.exports = function(RED) {
    "use strict";
    var settings = RED.settings;
    var events = require("events");

    function NRCSerialNode(n) {
        RED.nodes.createNode(this, n);
        this.name = "nrc-serial";
        this.serialport = n.serialport;
        //this.newline = n.newline;
        //this.addchar = n.addchar || "false";
        this.serialbaud = parseInt(n.serialbaud) || 57600;
        this.databits = parseInt(n.databits) || 8;
        this.parity = n.parity || "none";
        this.flowctrl = n.flowctrl || "none";
        this.stopbits = parseInt(n.stopbits) || 1;
        //this.bin = n.bin || "false";
        //this.out = n.out || "char";
    }
    RED.nodes.registerType("nrc-serial",NRCSerialNode);

    function NRCSerialOutNode(n) {
        RED.nodes.createNode(this,n);
        this.serial = n.serial;
        this.bufsize = n.bufsize;
        this.name = n.name;
        this.topic = n.topic;
        this.priority = n.priority;
        this.serialConfig = RED.nodes.getNode(this.serial);
    }
    RED.nodes.registerType("nrc-serial-out", NRCSerialOutNode);

    function NRCSerialInNode(n) {
        RED.nodes.createNode(this,n);
        this.serial = n.serial;
        this.name = n.name;
        this.topic = n.topic;
        this.priority = n.priority;
        this.msgtype = n.msgtype;
        this.bufsize = n.bufsize;
        this.serialConfig = RED.nodes.getNode(this.serial);
    }
    RED.nodes.registerType("nrc-serial-in", NRCSerialInNode);
}
