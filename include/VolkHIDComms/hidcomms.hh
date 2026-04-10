#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <string_view>

class HIDComms {
public:
    struct ComPort {
        std::string name;
        std::string device_path;
    };

    HIDComms();

    void auto_connect();
    void disconnect();

    bool write(const BYTE* data, DWORD length);
    bool write(std::string_view data);
    bool move(long x, long y);

    std::string read_response(DWORD timeout_ms = 500);

    bool is_connected() const { return com_port && com_port != INVALID_HANDLE_VALUE; }
    const std::vector<ComPort>& get_com_ports() const { return com_ports; }
    const std::string& get_connected_to() const { return connected_to; }
    const std::string& get_device() const { return device; }

private:
    bool find_com_ports();
    HANDLE open_com_port(const char* port_name);
    bool configure_com_port(HANDLE h_serial, DWORD baud_rate);

    std::vector<ComPort> com_ports;
    HANDLE com_port = 0;
    std::string connected_to;
    std::string device;
};
