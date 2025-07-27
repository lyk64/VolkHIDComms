#include "hidcomms.hh"

HIDComms::HIDComms() {
    find_com_ports();
}

HIDComms::~HIDComms() {
    connection.reset();
}

bool HIDComms::find_com_ports() {
    com_ports.clear();

    HKEY h_key = nullptr;
    constexpr std::string_view registry_path = "HARDWARE\\DEVICEMAP\\SERIALCOMM";

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, registry_path.data(), 0, KEY_READ, &h_key) != ERROR_SUCCESS) {
        std::println(std::cerr, "[HIDComms] Failed to open registry key: {}", registry_path);
        return false;
    }

    DWORD value_count = 0;
    DWORD max_value_name_len = 0;
    if (RegQueryInfoKeyA(h_key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &value_count, &max_value_name_len, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        RegCloseKey(h_key);
        std::println(std::cerr, "[HIDComms] Failed to query registry key info.");
        return false;
    }

    std::vector<char> value_name(max_value_name_len + 1);
    std::vector<BYTE> value_data(256);

    for (DWORD i = 0; i < value_count; ++i) {
        DWORD value_name_len = static_cast<DWORD>(value_name.size());
        DWORD value_data_len = static_cast<DWORD>(value_data.size());
        DWORD type = 0;

        if (RegEnumValueA(h_key, i, value_name.data(), &value_name_len, nullptr, &type, value_data.data(), &value_data_len) == ERROR_SUCCESS) {
            if (type == REG_SZ) {
                std::string port_name(reinterpret_cast<const char*>(value_data.data()), value_data_len - 1);
                std::string device_path(value_name.data(), value_name_len);
                com_ports.push_back(ComPort{ .name = std::move(port_name), .device_path = std::move(device_path) });
            }
        }
    }

    RegCloseKey(h_key);
    return true;
}

bool HIDComms::connect_to_com_port(const std::string& port_name, DWORD baud_rate) {
    HANDLE h = CreateFileA((R"(\\.\)" + port_name).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::println(std::cerr, "[HIDComms] Failed to open port '{}', error code: {}", port_name, GetLastError());
        return false;
    }

    DCB dcb{ dcb.DCBlength = sizeof(dcb) };

    if (!GetCommState(h, &dcb)) {
        std::println(std::cerr, "[HIDComms] Failed to get COM state for '{}', error: {}", port_name, GetLastError());
        CloseHandle(h);
        return false;
    }

    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    if (!SetCommState(h, &dcb)) {
        std::println(std::cerr, "[HIDComms] Failed to set COM state for '{}', error: {}", port_name, GetLastError());
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS timeouts{ .ReadIntervalTimeout = MAXDWORD };
    if (!SetCommTimeouts(h, &timeouts)) {
        std::println(std::cerr, "[HIDComms] Failed to set timeouts for '{}', error: {}", port_name, GetLastError());
        CloseHandle(h);
        return false;
    }

    connection.handle = h;
    connection.port_name = port_name;

    return true;
}

bool HIDComms::write(std::span<const std::byte> data) {
    if (!connection.is_open()) {
        std::println(std::cerr, "[HIDComms] Write attempt failed: not connected.");
        return false;
    }

    DWORD bytes_written = 0;
    const DWORD data_size = static_cast<DWORD>(data.size_bytes());

    if (!WriteFile(connection.handle, data.data(), data_size, &bytes_written, nullptr)) {
        std::println(std::cerr, "[HIDComms] WriteFile failed, error: {}", GetLastError());
        return false;
    }

    if (bytes_written != data_size) {
        std::println(std::cerr, "[HIDComms] Partial write: {} of {} bytes written.", bytes_written, data_size);
        return false;
    }

    return true;
}

std::string HIDComms::read_response(DWORD timeout_ms) {
    if (!connection.is_open()) {
        std::println(std::cerr, "[HIDComms] Read attempt failed: not connected.");
        return {};
    }

    std::string result;
    std::array<std::byte, 1024> buffer{};
    const ULONGLONG start_time = GetTickCount64();

    while ((GetTickCount64() - start_time) < timeout_ms) {
        DWORD bytes_read = 0;
        if (!ReadFile(connection.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)) {
            std::println(std::cerr, "[HIDComms] ReadFile failed, error: {}", GetLastError());
            break;
        }

        if (bytes_read > 0) {
            result.append(reinterpret_cast<const char*>(buffer.data()), bytes_read);
        }
        else {
            Sleep(10);
        }
    }

    return result;
}

bool HIDComms::auto_connect() {
    find_com_ports();

    constexpr DWORD initial_baud_rate = 115200;
    constexpr DWORD high_baud_rate = 4000000;
    constexpr std::string_view version_cmd = "km.version()\r\n";
    constexpr std::array<std::byte, 9> makcu_baud_change_cmd = {
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0x05}, std::byte{0x00},
        std::byte{0xA5}, std::byte{0x00}, std::byte{0x09}, std::byte{0x3D},
        std::byte{0x00}
    };

    auto send_version_and_read = [&]() -> std::string {
        if (!write(std::as_bytes(std::span(version_cmd.begin(), version_cmd.end()))))
            return {};
        return read_response(1000);
    };

    for (const auto& port : com_ports) {
        if (!connect_to_com_port(port.name, initial_baud_rate))
            continue;

        Sleep(100);
        std::string resp = send_version_and_read();

        if (resp.find("Ferrum") != std::string::npos) {
            connection.reset();
            if (!connect_to_com_port(port.name, high_baud_rate))
                continue;

            connection.device_type = "Ferrum";
            return true;
        }

        if (resp.find("MAKCU") != std::string::npos) {
            if (!write(std::as_bytes(std::span(makcu_baud_change_cmd)))) {
                connection.reset();
                continue;
            }

            DCB dcb{};
            dcb.DCBlength = sizeof(dcb);
            if (!GetCommState(connection.handle, &dcb)) {
                connection.reset();
                continue;
            }

            dcb.BaudRate = high_baud_rate;
            if (!SetCommState(connection.handle, &dcb)) {
                connection.reset();
                continue;
            }

            PurgeComm(connection.handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
            Sleep(100);

            std::string verify_resp = send_version_and_read();
            if (verify_resp.find("km.MAKCU") != std::string::npos) {
                connection.device_type = "Makcu";
                return true;
            }

            connection.reset();
            continue;
        }

        if (!resp.empty()) {
            connection.device_type = "Generic B+";
            return true;
        }

        connection.reset();
        if (!connect_to_com_port(port.name, high_baud_rate))
            continue;

        Sleep(100);
        resp = send_version_and_read();

        if (resp.find("Ferrum") != std::string::npos) {
            connection.device_type = "Ferrum";
            return true;
        }

        if (resp.find("km.MAKCU") != std::string::npos) {
            connection.device_type = "Makcu";
            return true;
        }

        connection.reset();
    }

    return false;
}

void HIDComms::ConnectionInfo::reset() {
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    port_name.clear();
    device_type.clear();
}

bool HIDComms::ConnectionInfo::is_open() const {
    return handle != INVALID_HANDLE_VALUE;
}