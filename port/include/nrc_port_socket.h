/**
  * Copyright 2017 Tomas Frisberg & Ola Bjorsne
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http ://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#ifndef _NRC_PORT_SOCKET_H_
#define _NRC_PORT_SOCKET_H_

#include "nrc_types.h"
#include "nrc_port.h"

typedef void* nrc_port_socket_t;

enum nrc_socket_protocol {
    NRC_PORT_SOCKET_UDP,
    NRC_PORT_SOCKET_TCP
};

typedef void (*nrc_port_socket_remote_connect_evt_t)(nrc_port_socket_t socket, void *context);

typedef void (*nrc_port_socket_event)(nrc_port_socket_t socket, s32_t status);

struct nrc_port_socket_callback_fcn {
    nrc_port_socket_event data_available;
    nrc_port_socket_event write_complete;
    nrc_port_socket_event connect_event;
    nrc_port_socket_event disconnect_event;
    nrc_port_socket_event error_event;
};

/**
* @brief Initialises the socket module.
*
* Must be called before any other function is called.
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_init(void);

/**
* @brief Create a socket
*
* @param protocol Defines if the created socket is a TCP or UDP socket
* @param context  User context, passed as arguments in socket calbacks
* @param socket    Output parameter where the socket identifier is stored.
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_create(enum nrc_socket_protocol protocol, void* context, nrc_port_socket_t *socket);

/**
* @brief Register callbacks to a socket
*
* @param socket   socket identifier from the create call
* @param callbacks callback structure
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_register(nrc_port_socket_t socket, struct nrc_port_socket_callback_fcn *callback);

/**
* @brief Close a socket
*
* @param socket   socket identifier from the create call
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_close(nrc_port_socket_t socket);

/**
* @brief Bind a socket to a port
*
* @param socket   socket identifier from the create call
* @param port     protocol port
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_bind(nrc_port_socket_t socket, u16_t port);

/**
* @brief Connect to a remote port
*
* @param socket   socket identifier from the create call
* @param address  address of host as a string. 
*                 Examples: "127.0.0.1", "www.google.com"
* @param port     protocol port
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_connect(nrc_port_socket_t socket, const s8_t *address, u16_t port);

/**
* @brief Listen for incoming connections
*
* @param socket   socket identifier from the create call
* @param callback Callback notifed when an incoming connection is receivced
* @param port     protocol port
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_socket_listen(nrc_port_socket_t socket, nrc_port_socket_remote_connect_evt_t callback);

/**
* @brief Write to socket
* 
* The registered write_complete callback is notifed when the data has been sent.
*
* @param socket    socket identifier from the create call
* @param buf       Pointer to tx buffer
* @param buf_size  Sixe of tx buffer
*
* @return NRC_R_OK if call is successful.
*/
u32_t nrc_port_socket_write(nrc_port_socket_t socket, u8_t *buf, u32_t buf_size);

/**
* @brief Get number of rx bytes currently stored in socket
*
* The registered write_complete callback is notifed when the data has been sent.
* @param socket    socket identifier from the create call
*
* @return NRC_R_OK if call is successful.
*/
u32_t nrc_port_socket_get_bytes(nrc_port_socket_t socket);

/**
* @brief Read data from socket
*
* The registered data_available callback is notifed when data is recevied on the socket.
* Use this function to read the data. Data is copied from socket to rx buf 
*
* @param socket    socket identifier from the create call
* @param buf       Ref to rx buffer 
* @param buf_size  Size of rx buffer
*
* @return NRC_R_OK if call is successful.
*/
u32_t nrc_port_socket_read(nrc_port_socket_t socket, u8_t *buf, u32_t buf_size);

#endif