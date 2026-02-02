#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <iostream>

class HIDCOMMS {
public:
    HIDCOMMS();

    bool find_com_ports();
    
	HANDLE open_com_port(const char* port_name);
	bool configure_com_port(HANDLE h_serial, DWORD baud_rate);

    HANDLE auto_connect();

    bool write(const BYTE* data, DWORD length);

    std::string read_response(DWORD timeout_ms = 500);

    bool is_connected() const { return com_port && com_port != INVALID_HANDLE_VALUE; }
    const std::string& get_connected_to() const { return connected_to; }
    const std::string& get_device() const { return device; }

    struct ComPort {
        std::string name;
        std::string device_path;
    };
    std::vector<ComPort> com_ports;
    HANDLE com_port = 0;

private:
    std::string connected_to;
    std::string device;
};