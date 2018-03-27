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

    function TcpIn(n) {
        RED.nodes.createNode(this,n);
        this.host = n.host;
        this.port = n.port * 1;
        this.topic = n.topic;
        //this.stream = (!n.datamode||n.datamode=='stream'); /* stream,single*/
        this.msgtype = n.msgtype||'buffer'; /* buffer,utf8,base64 */
        this.role = n.role || "server";
        this.closing = false;
        this.connected = false;
        var node = this;
        var count = 0;
    }
    RED.nodes.registerType("nrc-tcp-in",TcpIn);

    function TcpOut(n) {
        RED.nodes.createNode(this,n);
        this.host = n.host;
        this.port = n.port * 1;
        this.doend = n.end || false;
        this.beserver = n.beserver;
        this.name = n.name;
        this.closing = false;
        this.connected = false;
        var node = this;
    }
    RED.nodes.registerType("nrc-tcp-out",TcpOut);

    function TcpGet(n) {
        RED.nodes.createNode(this,n);
        this.server = n.server;
        this.port = Number(n.port);
        this.out = n.out;
        this.splitc = n.splitc;
    }
    RED.nodes.registerType("nrc-tcp-request",TcpGet);
}
