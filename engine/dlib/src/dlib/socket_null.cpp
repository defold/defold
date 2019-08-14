#include "socket.h"

namespace dmSocket
{
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
