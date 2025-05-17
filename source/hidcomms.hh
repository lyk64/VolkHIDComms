#pragma once
#include <windows.h>
#include <vector>
#include <iostream>

class HIDCOMMS {
public:
    HIDCOMMS();

    void find_com_ports();
    HANDLE connect_to_com_port(const char* port_name, DWORD baud_rate);

    bool is_connected() const { return com_port && com_port != INVALID_HANDLE_VALUE; }
    const std::string& get_connected_to() const { return connected_to; }

    struct ComPort {
        std::string name;
        std::string device_path;
    };
    std::vector<ComPort> com_ports;
    HANDLE com_port = 0;

private:
    std::string connected_to;
};