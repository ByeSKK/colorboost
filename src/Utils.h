#pragma once
#include <filesystem>
#include <string>

namespace util {

	std::wstring Utf8ToWide(const std::string& text);
	std::string WideToUtf8(const std::wstring& text);
	std::string ToLowerAscii(std::string text);

	// %APPDATA%\ColorBoost\config.json (the folder is created if missing)
	std::filesystem::path ConfigFilePath();

	std::wstring ExecutablePath();
	bool IsAutostartEnabled();
	bool SetAutostartEnabled(bool enable);

	// Executable name (lower case, UTF-8) of the foreground window's process.
	// Returns an empty string if unknown or if it belongs to ignorePid.
	std::string ForegroundProcessExe(unsigned long ignorePid);

}  // namespace util