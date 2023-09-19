#pragma once

#include "IExtension.h"

std::wstring Utf8ToWide(const std::string& utf8string);
std::string WideToUtf8(const std::wstring& widestring);

// Utility method that returns empty std::string when passed nullptr,
// as the EOS SDK sometimes returns nullptr when strings are not set.
std::string StrFromPtr(const char* str);

std::vector<ExtensionParameter> UnpackExtensionParameterArray(size_t paramCount, const ExtensionParameterPOD* paramArr);
std::vector<NamedExtensionParameterPOD> PackNamedExtensionParameters(const std::map<std::string, ExtensionParameter>& params);