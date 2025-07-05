#ifndef SFLISTCONFIG_H
#define SFLISTCONFIG_H

#include <vector>
#include <string>
#include "nlohmann/json.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

class SoundFont
{
public:
    std::string path;

    bool enabled = true;
    bool xgdrums = false;
    bool linattmod = false;
    bool lindecvol = false;
    bool nofx = false;
    bool minfx = true;
    bool nolimits = true;
    bool norampin = false;

    int32_t sbank = -1;
    int32_t spreset = -1;
    int32_t dbank = 0;
    int32_t dbanklsb = 0;
    int32_t dpreset = -1;
};

void to_json(json& j, const SoundFont& sf);
void from_json(const json& j, SoundFont& sf);

class SFListConfig
{
public:
    SFListConfig(char list);
    std::vector<SoundFont*> loadFromDisk();
    void storeToDisk(std::vector<SoundFont*> list);

private:
    json m_list;
    fs::path m_listPath;
};

#endif // SFLISTCONFIG_H
