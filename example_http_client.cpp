#include "http/http.h"
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <URL>\n";
    return 1;
  }
  auto url = argv[1];
  http::Client http;

  // GET
  http::Response res = http.get(url);
  std::cout << res;

  // async GET
  // auto future = http.get(url, [](http::Response res) { std::cout << res; });

  return 0;
}
