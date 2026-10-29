#include "parameters.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>
#include <filesystem>

void Parameters::evaluate() {

  // initialize iniEnums
  initializeEnums();

  const std::string configFile =
      std::filesystem::current_path().string() + "/config.ini";

  if (!std::filesystem::exists(configFile)) {
    spdlog::warn("Config.ini not found. Creating empty configuration...");

    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.SaveFile(configFile.c_str());
    if (rc < 0) {
      spdlog::critical("Failed to create {}", configFile);
      exit(0);
      return;
    }
  }
  ///

  CSimpleIniA ini;
  ini.SetUnicode();

  SI_Error rc = ini.LoadFile(configFile.c_str());
  if (rc < 0) {
    spdlog::info("Failed to load Config.ini");
    return;
  }

  bool anyFailed = false;

  // anyFailed |= LoadSetting(ini, "Renderer", "VSync", renderingParameters.VSync);
  anyFailed |= LoadSetting(ini, "Device", "name", DeviceName);
  anyFailed |= LoadSetting(ini, "Ui", "svgPath", svgPath);
  anyFailed |= LoadSetting(ini, "General", "updateMs", updateMs);

  // // Read the configuration
  // read();

  // exit(0);
  if (anyFailed) {

    SI_Error lrc = ini.SaveFile("Config.ini");

    if (lrc < 0) {
      spdlog::critical(
          "[Parameters] Failed to save Config.ini (error code: {})", rc);
    } else {
      spdlog::warn(
          "[Parameters] Some values were not set. Reconstructed Config.ini.");
    }
  }
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, int &value) {
  if (ini.GetValue(section, key) == nullptr) {
    spdlog::error("[Parameters] Error Loading {} , {} , {}", section, key,
                  value);
    ini.SetLongValue(section, key, value);
    return true;
  }

  value = static_cast<int>(ini.GetLongValue(section, key, value));
  spdlog::info("[Parameters] Sucessfuly read {} , {} ,{} ", section, key,
               value);
  return false;
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, uint32_t &value) {

  if (ini.GetValue(section, key) == nullptr) {
    spdlog::error("[Parameters] Error Loading {} , {} , {}", section, key,
                  value);
    ini.SetLongValue(section, key, static_cast<long>(value));
    return true;
  }

  value = static_cast<uint32_t>(
      ini.GetLongValue(section, key, static_cast<long>(value)));
  spdlog::info("[Parameters] Sucessfuly read {} , {} ,{} ", section, key,
               value);

  if ((value & 0x80000000u) != 0) {

    spdlog::critical("[Parameters] Error loading {} , {} as uint32_t with "
                     "value {} has msb set to 1 may be a negative int.",
                     section, key, value);
    exit(0);
  }

  return false;
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, float &value) {
  if (ini.GetValue(section, key) == nullptr) {
    spdlog::error("[Parameters] Error Loading {} , {} , {}", section, key,
                  value);
    ini.SetDoubleValue(section, key, value);
    return true;
  }

  double tval = static_cast<double>(value);
  tval = ini.GetDoubleValue(section, key, tval);
  value = static_cast<float>(tval);
  spdlog::info("[Parameters] Sucessfuly read {} , {} ,{} ", section, key,
               value);
  return false;
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, bool &value) {
  if (ini.GetValue(section, key) == nullptr) {
    spdlog::error("[Parameters] Error Loading {} , {} , {}", section, key,
                  value);
    ini.SetBoolValue(section, key, value);
    return true;
  }

  value = ini.GetBoolValue(section, key, value);
  spdlog::info("[Parameters] Sucessfuly read {} , {} ,{} ", section, key,
               value);
  return false;
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, std::string &value) {
  if (ini.GetValue(section, key) == nullptr) {
    spdlog::error("[Parameters] Error Loading {} , {} , {}", section, key,
                  value);
    ini.SetValue(section, key, value.c_str());
    return true;
  }

  value = ini.GetValue(section, key, value.c_str());
  spdlog::info("[Parameters] Sucessfuly read {} , {} ,{} ", section, key,
               value);
  return false;
}

bool Parameters::LoadSetting(CSimpleIniA &ini, const char *section,
                             const char *key, iniEnum &inienum) {

  if (!inienum.init) {

    spdlog::critical("[Parameters] tryed to access uninit enum");
    exit(0);
  }
  const char *iniValue = ini.GetValue(section, key, nullptr);

  // Setting does not exist
  if (iniValue == nullptr) {
    // Use currently selected value as the default
    const std::string &defaultValue = inienum.SelectedValue.second;

    spdlog::warn("[Parameters] Missing {}, {}. Using default: {}", section, key,
                 defaultValue);

    // Write default string to INI
    ini.SetValue(section, key, defaultValue.c_str());

    return true;
  }

  // Find matching enum value by name
  auto it =
      std::find_if(inienum.Values.begin(), inienum.Values.end(),
                   [&](const auto &entry) { return entry.second == iniValue; });

  if (it != inienum.Values.end()) {
    // Found it
    inienum.SelectedValue = *it;

    spdlog::info("[Parameters] Successfully read {}, {} = {} ({})", section,
                 key, inienum.SelectedValue.first,
                 inienum.SelectedValue.second);

    return false;
  }

  // Invalid value in INI
  spdlog::warn("[Parameters] Invalid value {}, {} = {}. Using default: {}",
               section, key, iniValue, inienum.SelectedValue.second);

  // Restore default
  ini.SetValue(section, key, inienum.SelectedValue.second.c_str());

  return true;
}

void Parameters::initializeEnums() {

}
