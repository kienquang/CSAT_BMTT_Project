#include "EnvConfig.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace std;

namespace {

unordered_map<string, string> g_envValues;
once_flag g_loadFlag;

string Trim(const string& value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "";
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

string StripQuotes(const string& value) {
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

filesystem::path FindEnvFile() {
    filesystem::path current = filesystem::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        const filesystem::path candidate = current / ".env";
        if (filesystem::exists(candidate)) {
            return candidate;
        }

        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return {};
}

void LoadEnvFile() {
    const filesystem::path envPath = FindEnvFile();
    if (envPath.empty()) {
        return;
    }

    ifstream input(envPath);
    string line;
    while (getline(input, line)) {
        string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const size_t separator = trimmed.find('=');
        if (separator == string::npos) {
            continue;
        }

        const string key = Trim(trimmed.substr(0, separator));
        const string value = StripQuotes(Trim(trimmed.substr(separator + 1)));
        if (!key.empty()) {
            g_envValues[key] = value;
        }
    }
}

const string& GetLoadedValue(const string& key) {
    call_once(g_loadFlag, LoadEnvFile);
    static const string kEmpty;

    const auto it = g_envValues.find(key);
    if (it == g_envValues.end()) {
        return kEmpty;
    }

    return it->second;
}

} // namespace

string EnvConfig::GetString(const string& key, const string& defaultValue) {
    const char* processValue = getenv(key.c_str());
    if (processValue != nullptr && processValue[0] != '\0') {
        return processValue;
    }

    const string& fileValue = GetLoadedValue(key);
    if (!fileValue.empty()) {
        return fileValue;
    }

    return defaultValue;
}

int EnvConfig::GetInt(const string& key, int defaultValue) {
    const string value = GetString(key);
    if (value.empty()) {
        return defaultValue;
    }

    try {
        return stoi(value);
    } catch (...) {
        return defaultValue;
    }
}
