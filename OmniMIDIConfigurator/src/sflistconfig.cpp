#include "sflistconfig.h"
#include "utils.h"
#include <format>
#include <fstream>
#include <iostream>

void to_json(json& out, const SoundFont& sf) {
    out["path"] = sf.path;
    out["enabled"] = sf.enabled;
    out["xgdrums"] = sf.xgdrums;
    out["linattmod"] = sf.linattmod;
    out["lindecvol"] = sf.lindecvol;
    out["nofx"] = sf.nofx;
    out["minfx"] = sf.minfx;
    out["nolimits"] = sf.nolimits;
    out["norampin"] = sf.norampin;
    out["sbank"] = sf.sbank;
    out["spreset"] = sf.spreset;
    out["dbank"] = sf.dbank;
    out["dbanklsb"] = sf.dbanklsb;
    out["dpreset"] = sf.dpreset;
}

void from_json(const json& obj, SoundFont& sf) {
    sf.path = obj.value("path", sf.path);
    sf.enabled = obj.value("enabled", sf.enabled);
    sf.xgdrums = obj.value("xgdrums", sf.xgdrums);
    sf.linattmod = obj.value("linattmod", sf.linattmod);
    sf.lindecvol = obj.value("lindecvol", sf.lindecvol);
    sf.nofx = obj.value("nofx", sf.nofx);
    sf.minfx = obj.value("minfx", sf.minfx);
    sf.nolimits = obj.value("nolimits", sf.nolimits);
    sf.norampin = obj.value("norampin", sf.norampin);
    sf.sbank = obj.value("sbank", sf.sbank);
    sf.spreset = obj.value("spreset", sf.spreset);
    sf.dbank = obj.value("dbank", sf.dbank);
    sf.dbanklsb = obj.value("dbanklsb", sf.dbanklsb);
    sf.dpreset = obj.value("dpreset", sf.dpreset);
}

SFListConfig::SFListConfig(char list) {
    std::string filename = std::format("OmniMIDI_{0}.json", list);
    m_listPath = Utils::GetConfigFolder() / "lists" / filename;
}

std::vector<SoundFont*> SFListConfig::loadFromDisk() {
    std::vector<SoundFont*> list = {};

    std::ifstream ifs(m_listPath);
    if (!ifs)
        throw std::runtime_error("Error opening list file.");

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    try {
        m_list = json::parse(content, nullptr, false, true);
        json jsonList = m_list["SoundFonts"];

        for (json sf : jsonList) {
            SoundFont *n = new SoundFont;
            from_json(sf, *n);
            list.push_back(n);
        }
    } catch (const std::exception &e) {
        std::string s = "Error loading list:";
        throw std::runtime_error(s + e.what());
    }

    return list;
}

void SFListConfig::storeToDisk(std::vector<SoundFont*> list) {
    json outJson;
    std::vector<SoundFont> toWrite;
    for (auto sf : list)
        toWrite.push_back(*sf);

    outJson["SoundFonts"] = toWrite;

    std::ofstream out(m_listPath);
    if (!out)
        throw std::runtime_error("Error creating settings file.");

    out << outJson.dump(4);
    out.close();

    if (out.bad() || out.fail())
        throw std::runtime_error("Error writing to settings file.");
}
