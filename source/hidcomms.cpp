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