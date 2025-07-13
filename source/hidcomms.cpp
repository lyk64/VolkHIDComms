#include "hidcomms.hh"

HIDCOMMS::HIDCOMMS() {
    find_com_ports();
}

bool HIDCOMMS::find_com_ports() {
    com_ports.clear();

    HKEY h_key;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_READ, &h_key) != ERROR_SUCCESS) {
        std::cerr << "[HIDCOMMS] Failed to open registry key! Likely no aim device connected." << "\n";
        return false;
    }

    DWORD value_count, max_value_name_length;
    if (RegQueryInfoKey(h_key, NULL, NULL, NULL, NULL, NULL, NULL, &value_count, &max_value_name_length, NULL, NULL, NULL) != ERROR_SUCCESS) {
        RegCloseKey(h_key);
        return false;
    }

    std::vector<char> value_name(max_value_name_length + 1);
    std::vector<BYTE> value_data(256);
    DWORD value_name_length, value_data_length, type;

    for (DWORD i = 0; i < value_count; i++) {
        value_name_length = max_value_name_length + 1;
        value_data_length = 256;

        if (RegEnumValueA(h_key, i, value_name.data(), &value_name_length, NULL, &type, value_data.data(), &value_data_length) == ERROR_SUCCESS) {
            if (type == REG_SZ) {
                ComPort port;
                port.name = std::string(reinterpret_cast<char*>(value_data.data()));
                port.device_path = std::string(value_name.data());
                com_ports.push_back(port);
            }
        }
    }

    RegCloseKey(h_key);

    return true;
}

HANDLE HIDCOMMS::connect_to_com_port(const char* port_name, DWORD baud_rate) {
    HANDLE h_serial = CreateFileA(("\\\\.\\" + std::string(port_name)).c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, 0, NULL);

    if (h_serial == INVALID_HANDLE_VALUE) {
        std::cerr << "[HIDCOMMS] Failed to open port " << port_name << ", error code: " << GetLastError() << "\n";
        return (HANDLE)-1;
    }

    DCB dcb_serial_parameters = { 0 };
    dcb_serial_parameters.DCBlength = sizeof(dcb_serial_parameters);

    if (!GetCommState(h_serial, &dcb_serial_parameters)) {
        std::cerr << "[HIDCOMMS] Failed to get COM state, error code: " << GetLastError() << "\n";
        CloseHandle(h_serial);
        return (HANDLE)-2;
    }

    dcb_serial_parameters.BaudRate = baud_rate;
    dcb_serial_parameters.ByteSize = 8;
    dcb_serial_parameters.StopBits = ONESTOPBIT;
    dcb_serial_parameters.Parity = NOPARITY;

    if (!SetCommState(h_serial, &dcb_serial_parameters)) {
        std::cerr << "[HIDCOMMS] Failed to set COM state, error code: " << GetLastError() << "\n";
        CloseHandle(h_serial);
        return (HANDLE)-3;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(h_serial, &timeouts)) {
        std::cerr << "[HIDCOMMS] Failed to set timeouts, error code: " << GetLastError() << "\n";
        CloseHandle(h_serial);
        return (HANDLE)-4;
    }

    connected_to = port_name;
    return h_serial;
}

bool HIDCOMMS::write(const BYTE* data, DWORD length) {
    if (!is_connected()) return false;

    DWORD bytes_written;
    if (!WriteFile(com_port, data, length, &bytes_written, NULL) || bytes_written != length) {
        std::cerr << "[HIDCOMMS] Failed to write, error code: " << GetLastError() << "\n";
        return false;
    }
    return true;
}

std::string HIDCOMMS::read_response(DWORD timeout_ms) {
    if (!is_connected()) return "";

    std::string result;
    BYTE buffer[1024];
    ULONGLONG start_time = GetTickCount64();

    while (GetTickCount64() - start_time < timeout_ms) {
        DWORD bytes_read;
        if (ReadFile(com_port, buffer, sizeof(buffer), &bytes_read, NULL)) {
            if (bytes_read > 0) {
                result.append(reinterpret_cast<char*>(buffer), bytes_read);
            }
            else {
                Sleep(10);
            }
        }
        else {
            std::cerr << "[HIDCOMMS] Failed to read, error code: " << GetLastError() << "\n";
            break;
        }
    }

    return result;
}

HANDLE HIDCOMMS::auto_connect() {
    find_com_ports();

    const DWORD initial_baud_rate = 115200;
    const DWORD high_baud_rate = 4000000;
    std::string version_cmd = "km.version()\r\n";

    BYTE makcu_baud_change_cmd[9] = { 0xDE, 0xAD, 0x05, 0x00, 0xA5, 0x00, 0x09, 0x3D, 0x00 };

    for (const auto& port : com_ports) {
        HANDLE h = connect_to_com_port(port.name.c_str(), initial_baud_rate);
        if ((intptr_t)h < 0) {
            continue;
        }

        com_port = h;

        Sleep(100);

        if (!write(reinterpret_cast<const BYTE*>(version_cmd.data()), version_cmd.size())) {
            CloseHandle(h);
            com_port = 0;
            continue;
        }

        std::string resp = read_response(1000);

        if (resp.find("Ferrum") != std::string::npos) {
            CloseHandle(h);
            com_port = 0;

            h = connect_to_com_port(port.name.c_str(), high_baud_rate);
            if ((intptr_t)h < 0) {
                continue;
            }

            com_port = h;
            device = "Ferrum";
            return com_port;
        }
        else if (resp.find("MAKCU") != std::string::npos) {
            if (!write(makcu_baud_change_cmd, sizeof(makcu_baud_change_cmd))) {
                CloseHandle(h);
                com_port = 0;
                continue;
            }

            std::string change_resp = read_response(1000);

            DCB dcb;
            memset(&dcb, 0, sizeof(dcb));
            dcb.DCBlength = sizeof(dcb);

            if (!GetCommState(h, &dcb)) {
                CloseHandle(h);
                com_port = 0;
                continue;
            }

            dcb.BaudRate = high_baud_rate;

            if (!SetCommState(h, &dcb)) {
                CloseHandle(h);
                com_port = 0;
                continue;
            }

            PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

            Sleep(100);

            if (!write(reinterpret_cast<const BYTE*>(version_cmd.data()), version_cmd.size())) {
                CloseHandle(h);
                com_port = 0;
                continue;
            }

            std::string verify_resp = read_response(1000);

            if (verify_resp.find("km.MAKCU") != std::string::npos) {
                device = "Makcu";
                return com_port;
            }
            else {
                CloseHandle(h);
                com_port = 0;
                continue;
            }
        }
        else {
            if (!resp.empty()) {
                device = "Generic B+";
                return com_port;
            }
            else {
                CloseHandle(h);
                com_port = 0;

                h = connect_to_com_port(port.name.c_str(), high_baud_rate);
                if ((intptr_t)h < 0) {
                    continue;
                }

                com_port = h;

                Sleep(100);

                if (!write(reinterpret_cast<const BYTE*>(version_cmd.data()), version_cmd.size())) {
                    CloseHandle(h);
                    com_port = 0;
                    continue;
                }

                resp = read_response(1000);

                if (resp.find("Ferrum") != std::string::npos) {
                    device = "Ferrum";
                    return com_port;
                }
                else if (resp.find("km.MAKCU") != std::string::npos) {
                    device = "Makcu";
                    return com_port;
                }
                else {
                    CloseHandle(h);
                    com_port = 0;
                    continue;
                }
            }
        }
    }

    return (HANDLE)-5;
}