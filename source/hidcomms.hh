#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <span>
#include <array>

class HIDComms {
public:
    HIDComms();
    ~HIDComms();

    bool find_com_ports();
    bool connect_to_com_port(const std::string& port_name, DWORD baud_rate);
    bool write(std::span<const std::byte> data);
    [[nodiscard]] std::string read_response(DWORD timeout_ms = 500);
    bool auto_connect();

    struct ComPort {
        std::string name;
        std::string device_path;
    };
    std::vector<ComPort> com_ports;

    struct ConnectionInfo {
        HANDLE handle = INVALID_HANDLE_VALUE;
        std::string port_name;
        std::string device_type;

        void reset();
        bool is_open() const;
    } connection;
};