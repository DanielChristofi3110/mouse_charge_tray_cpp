
#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <SimpleIni.h>
#include <utility>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunique-object-duplication"
#endif

struct iniEnum {

  std::vector<std::pair<uint32_t, std::string>> Values;
  std::pair<uint32_t, std::string> SelectedValue;
  bool init = false;
};

class Parameters {
public:
  static Parameters &get() {
    static Parameters s_instance;
    return s_instance;
  }

  std::string DeviceName="Meetion_MS_BT1";
  std::string svgPath="mouse.svg";
  uint32_t updateMs=30'000;
  bool logging = true;

  void evaluate();

private:
  Parameters() = default;

  void initializeEnums();

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   int &value);

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   float &value);

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   uint32_t &value);

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   bool &value);

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   std::string &value);

  bool LoadSetting(CSimpleIniA &ini, const char *section, const char *key,
                   iniEnum &inienum);
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif
