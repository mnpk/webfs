#include "json/json.h"
#include <fstream>
#include <streambuf>

struct Config {
  using json = nlohmann::json;
  json j;
  Config(const std::string& path) {
    // load config file
    std::ifstream ifs(path);
    std::string raw((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
    ifs.close();
    if (!raw.empty()) {
      j = json::parse(raw);
    }

    // set default values
    if (j["origin"].is_null()) {
      j["origin"] = "http://alice";
    }
    if (j["log"].is_null()) {
      j["log"] = "./webfs.log";
    }

    // save config file
    std::ofstream ofs(path, std::ios::out|std::ios::trunc);
    ofs << j.dump(2) << std::endl;
    ofs.close();
  }

  std::string origin() {
    return j["origin"];
  }

  std::string log() {
    return j["log"];
  }
};
