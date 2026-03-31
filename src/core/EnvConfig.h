#ifndef ENV_CONFIG_H
#define ENV_CONFIG_H

#include <string>

class EnvConfig {
public:
    static std::string GetString(const std::string& key, const std::string& defaultValue = "");
    static int GetInt(const std::string& key, int defaultValue);
};

#endif // ENV_CONFIG_H
