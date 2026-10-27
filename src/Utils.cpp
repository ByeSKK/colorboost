#include "Utils.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <iterator>

namespace fs = std::filesystem;

namespace util {

    namespace {
        constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        constexpr wchar_t kRunValue[] = L"ColorBoost";
    }  // namespace

    std::wstring Utf8ToWide(const std::string& text) {
        if (text.empty()) return {};
        const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
        return result;
    }

    std::string WideToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0) return {};
        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
        return result;
    }

    std::string ToLowerAscii(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
            });
        return text;
    }

    fs::path ConfigFilePath() {
        fs::path dir;
        PWSTR known = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &known)) && known) {
            dir = known;
            CoTaskMemFree(known);
        }
        else {
            std::error_code ec;
            dir = fs::temp_directory_path(ec);
        }
        dir /= L"ColorBoost";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir / L"config.json";
    }

    std::wstring ExecutablePath() {
        wchar_t buffer[MAX_PATH * 2] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        return std::wstring(buffer, length);
    }

    bool IsAutostartEnabled() {
        DWORD size = 0;
        return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) == ERROR_SUCCESS;
    }

    bool SetAutostartEnabled(bool enable) {
        if (enable) {
            const std::wstring value = L"\"" + ExecutablePath() + L"\" --minimized";
            const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
            return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ, value.c_str(), bytes) == ERROR_SUCCESS;
        }
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    std::string ForegroundProcessExe(unsigned long ignorePid) {
        HWND foreground = GetForegroundWindow();
        if (!foreground) return {};

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        if (pid == 0 || pid == ignorePid) return {};

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return {};

        wchar_t buffer[MAX_PATH * 2] = {};
        DWORD length = static_cast<DWORD>(std::size(buffer));
        std::string result;
        if (QueryFullProcessImageNameW(process, 0, buffer, &length)) {
            const std::wstring full(buffer, length);
            const size_t slash = full.find_last_of(L"\\/");
            const std::wstring name = (slash == std::wstring::npos) ? full : full.substr(slash + 1);
            result = ToLowerAscii(WideToUtf8(name));
        }
        CloseHandle(process);
        return result;
    }

}  // namespace util