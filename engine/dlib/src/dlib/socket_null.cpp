// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "socket.h"

namespace dmSocket
{
    Selector::Selector()
    {
    }

    Result Initialize() {
        return RESULT_OK;
    }

    Result Finalize() {
        return RESULT_OK;
    }

    Result New(Type type, enum Protocol protocol, Socket* socket) {
        return RESULT_OPNOTSUPP;
    }

    Result Delete(Socket socket) {
        return RESULT_OPNOTSUPP;
    }

    int GetFD(Socket socket) {
        return -1;
    }

    Result SetReuseAddress(Socket socket, bool reuse) {
        return RESULT_OPNOTSUPP;
    }

    Result SetBroadcast(Socket socket, bool broadcast) {
        return RESULT_OPNOTSUPP;
    }

    Result SetBlocking(Socket socket, bool blocking) {
        return RESULT_OPNOTSUPP;
    }

    Result SetNoDelay(Socket socket, bool no_delay) {
        return RESULT_OPNOTSUPP;
    }

    Result AddMembership(Socket socket, Address multi_addr, Address interface_addr, int ttl) {
        return RESULT_OPNOTSUPP;
    }

    Result Accept(Socket socket, Address* address, Socket* accept_socket) {
        return RESULT_OPNOTSUPP;
    }

    Result Bind(Socket socket, Address address, int port) {
        return RESULT_OPNOTSUPP;
    }

    Result Connect(Socket socket, Address address, int port) {
        return RESULT_OPNOTSUPP;
    }

    Result Listen(Socket socket, int backlog) {
        return RESULT_OPNOTSUPP;
    }

    Result Shutdown(Socket socket, ShutdownType how) {
        return RESULT_OPNOTSUPP;
    }

    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes) {
        return RESULT_OPNOTSUPP;
    }

    Result SendTo(Socket socket, const void* buffer, int length, int* sent_bytes, Address to_addr, uint16_t to_port) {
        return RESULT_OPNOTSUPP;
    }

    Result Receive(Socket socket, void* buffer, int length, int* received_bytes) {
        return RESULT_OPNOTSUPP;
    }

    Result ReceiveFrom(Socket socket, void* buffer, int length, int* received_bytes,
                       Address* from_addr, uint16_t* from_port) {
        return RESULT_OPNOTSUPP;
    }

    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket) {
    }

    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket) {
    }

    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket) {
        return false;
    }

    void SelectorZero(Selector* selector) {
    }

    Result Select(Selector* selector, int32_t) {
        return RESULT_OPNOTSUPP;
    }

    Result GetName(Socket socket, Address*address, uint16_t* port) {
        return RESULT_OPNOTSUPP;
    }

    Result GetHostname(char* hostname, int hostname_length) {
        return RESULT_OPNOTSUPP;
    }

    Result GetLocalAddress(Address* address) {
        return RESULT_OPNOTSUPP;
    }

    Address AddressFromIPString(const char* address) {
        return Address();
    }

    char* AddressToIPString(Address address) {
        return 0;
    }

    Result GetHostByName(const char* name, Address* address) {
        return RESULT_OPNOTSUPP;
    }

    const char* ResultToString(Result result) {
        return "NOT SUPPORTED";
    }

}
