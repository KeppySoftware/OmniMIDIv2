#ifndef UTILS_H
#define UTILS_H

#include <filesystem>

#ifdef _WIN32
#include <vector>
#endif

#define CCS(b) b ? Qt::CheckState::Checked : Qt::CheckState::Unchecked

#define FATAL_ERROR_TITLE   "OmniMIDI | Fatal Error"
#define ERROR_TITLE         "OmniMIDI | Error"
#define WARNING_TITLE       "OmniMIDI | Warning"

namespace fs = std::filesystem;

typedef struct {
    std::string bass;
    std::string bassmidi;
    std::string xsynth;
    std::string fluidsynth;
} SynthVersions;

namespace Utils
{
fs::path GetConfigFolder();
void InitializeKdmapi();
std::string versionToString(int ver);
SynthVersions getSynthVersions();

#ifdef _WIN32
std::vector<std::string> getASIODevices();
#endif
}

#endif // UTILS_H
