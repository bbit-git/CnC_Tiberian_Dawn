/*
 * TCP/IP stub for Android port.
 * Original used Win32 Winsock — will be rebuilt with modern networking later.
 */
#ifndef TCPIP_H
#define TCPIP_H

#include "td_platform.h"

class TcpipManagerClass {
public:
    TcpipManagerClass() {}
    int Num_Connections() { return 0; }
    int Is_Connected() { return 0; }
    int Open_Socket(int) { return 0; }
    void Close_Socket() {}
    int Start_Listening() { return 0; }
    int Connect(const char*, int) { return 0; }
};

extern TcpipManagerClass Winsock;

#endif
