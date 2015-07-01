#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include "fusecpp/fusecpp.h"
#include "httpp/include/http.h"

using namespace fuse_cpp;

static std::string origin = "http://alice";
static std::ofstream log;

static int http_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else {
    // log << "url:" << url << std::endl;
    std::string url = origin + std::string(path);
    log << "[getattr] path:" << path << " url:" << url << "\n";
    try {
      http::Client http;
      http::Response res = http.head(url);
      if (res.code_ != 200)
        return -ENOENT;
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = atoi(res.headers_["Content-Length"].c_str());
      log << "[getattr] size:" << atoi(res.headers_["Content-Length"].c_str())
          << "\n";
    } catch (std::runtime_error &err) {
      log << "[getattr] failed to request " << url << ", error:" << err.what()
          << std::endl;
      return -ENOENT;
    }
  }
  return 0;
}

static int http_open(const char *path, struct fuse_file_info *fi) {
  log << "[open] path:" << path << "\n";
  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;

  http::Client http;
  std::string url = origin + std::string(path);
  try {
    http::Response res = http.head(url);
    if (res.code_ != 200)
      return -ENOENT;
  } catch (std::runtime_error &err) {
    log << "[open] failed to request " << url << ", error:" << err.what()
        << std::endl;
    return -ENOENT;
  }
  return 0;
}

static int http_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info * /*fi*/) {
  // (void)fi;
  log << "[read] path:" << path << " size:" << size << " offset:" << offset
      << std::endl;
  ;
  std::string url = origin + std::string(path);
  try {
    std::stringstream range_value;
    range_value << "bytes=" << offset << "-" << offset + size;
    http::Client http;
    http::Response res = http.get(url, {{"Range", range_value.str()}});
    if (res.code_ != 200 && res.code_ != 206) {
      log << "[read] http response is not OK. url:" << url
          << " code:" << res.code_ << " reason:" << res.reason_ << std::endl;
      return -ENOENT;
    }
    std::string content = res.contentStr();
    if (size > content.length())
      size = content.length();
    memcpy(buf, content.c_str(), size);
  } catch (std::runtime_error &err) {
    log << "[read] failed to request " << url << ", error:" << err.what()
        << std::endl;
    return -ENOENT;
  }

  return size;
}

int main(int argc, char *argv[]) {
  log.open("/home/mnpk/src/wfs/log.txt");
  if (argc != 2) {
    printf("Usage: %s <dir>\n", argv[0]);
    return 1;
  }
  FuseDispatcher *dispatcher;
  dispatcher = new FuseDispatcher();
  dispatcher->set_getattr(&http_getattr);
  dispatcher->set_open(&http_open);
  dispatcher->set_read(&http_read);
  return fuse_main(argc, argv, (dispatcher->get_fuseOps()), NULL);
}
