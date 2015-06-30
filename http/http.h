#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <future>

namespace http {

struct Response {
  std::string status_line_;
  int code_;
  std::string reason_;
  std::unordered_map<std::string, std::string> headers_;
  std::vector<char> content_;

  Response() {
  }
  Response(std::string line) {
    status_line_ = line;
    line = line.substr(line.find(" ") + 1);
    code_ = atoi(line.substr(0, line.find(" ")).c_str());
    reason_ = line.substr(line.find(" "));
  }
  void setHeader(std::string line) {
    std::string field = line.substr(0, line.find(":"));
    line = line.substr(line.find(":") + 1);
    std::string value = line.substr(line.find_first_not_of(" "));
    headers_.emplace(field, value);
  }
  std::string contentStr() {
    return std::string(content_.begin(), content_.end());
  }
};

std::ostream& operator<<(std::ostream& os, const Response& res) {
  os << res.status_line_ << std::endl;
  for (auto& h : res.headers_) {
    os << h.first << ": " << h.second << std::endl;
  }
  os << std::endl;
  for (auto& c : res.content_) {
    os << c;
  }
  return os;
}

struct Connection {
  int sock_ = -1;
  char buffer_[1024];
  int head_ = 0;
  int tail_ = 0;

  Connection(const std::string& host, int port) {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == -1) {
      std::stringstream ss;
      ss << "socket() failed. errno:" << errno << std::endl;
      throw std::runtime_error(ss.str());
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_addr(host.c_str()) == INADDR_NONE) {
      struct hostent *he = gethostbyname(host.c_str());
      if (he == NULL) {
        throw std::runtime_error("failed to resolve hostname");
      }
      server_addr.sin_addr.s_addr = *((long int*)*he->h_addr_list);
    } else {
      server_addr.sin_addr.s_addr = inet_addr(host.c_str());
    }
    server_addr.sin_port = htons(port);
    if (connect(sock_, reinterpret_cast<struct sockaddr*>(&server_addr),
                sizeof(server_addr)) == -1) {
      std::stringstream ss;
      ss << "connect() failed. errno: " << errno << std::endl;
      throw std::runtime_error(ss.str());
    }
  }
  int sendRequest(const std::string& req) {
    if (sock_ < 0) {
      throw std::runtime_error("cannot send(): sock_ < 0");
    }
    int nsent = send(sock_, req.c_str(), req.length(), 0);
    // cout << req << std::endl;
    return nsent;
  }

  std::string recvLine() {
    std::string res;
    while (true) {
      char* eol = strstr(&buffer_[head_], "\r\n");
      if (eol != NULL) {
        memset(eol, 0, 2);
        res += std::string(&buffer_[head_]);
        head_ = eol + 2 - buffer_;
        break;
      } else {
        int buffered = tail_ - head_;
        res += std::string(&buffer_[head_], buffered);
        head_ = 0;
      }

      int nread = recv(sock_, buffer_, 1024, 0);
      if (nread == -1) {
        std::stringstream ss;
        ss << "recv() failed. errno: " << errno << std::endl;
        throw std::runtime_error(ss.str());
      }
      tail_ = nread;
    }
    return res;
  }

  std::vector<char> recvN(int n) {
    std::vector<char> res;
    int ntoread = n;
    while (true) {
      int buffered = tail_ - head_;
      if (buffered >= ntoread) {
        res.insert(res.end(), &buffer_[head_], &buffer_[head_ + ntoread]);
        head_ += ntoread;
        break;
      } else {
        // res += std::string(&buffer_[head_], buffered);
        res.insert(res.end(), &buffer_[head_], &buffer_[head_ + buffered]);
        ntoread -= buffered;
        head_ = 0;
        tail_ = 0;
      }
      int nread = recv(sock_, buffer_, 1024, 0);
      if (nread == -1) {
        std::stringstream ss;
        ss << "recv() failed. errno: " << errno << std::endl;
        throw std::runtime_error(ss.str());
      }
      tail_ = nread;
    }
    return res;
  }

  Response recvHeader() {
    std::string line = recvLine();
    Response res(line);
    while (true) {
      line = recvLine();
      if (line.empty()) {
        break;
      }
      res.setHeader(line);
    }
    return res;
  }

  Response& recvContent(Response& res) {
    if (res.headers_.count("Content-Length") > 0) {
      int length = atoi(res.headers_["Content-Length"].c_str());
      res.content_ = recvN(length);
    } else if (res.headers_.count("Transfer-Encoding") > 0) {
      if (res.headers_["Transfer-Encoding"] == "chunked") {
        while (true) {
          int size = strtol(recvLine().c_str(), NULL, 16);
          if (size == 0)
            break;
          std::vector<char> chunk = recvN(size);
          res.content_.insert(res.content_.end(), chunk.begin(), chunk.end());
        }
      }
    }
    return res;
  }
};

struct URL {
  std::string str_;
  std::string scheme_ = "http";
  std::string host_;
  int port_ = 80;
  std::string path_;
  std::string query_;

  URL(std::string url) {
    str_ = url;
    size_t host_start = url.find("://");
    if (host_start != std::string::npos) {
      scheme_ = url.substr(0, host_start);
      host_start += 3;
    } else {
      host_start = 0;
    }
    url = url.substr(host_start);
    size_t host_end = url.find_first_of("/");
    host_ = url.substr(0, host_end);
    size_t colon_in_host = host_.find(":");
    if (colon_in_host != std::string::npos) {
      port_ = atoi(host_.substr(colon_in_host + 1).c_str());
      host_ = host_.substr(0, colon_in_host);
    }
    if (host_end != std::string::npos) {
      path_ = url.substr(url.find_first_of("/"));
    } else {
      path_ = "/";
    }
    size_t query_start = path_.find_first_of("?");
    if (query_start != std::string::npos) {
      query_ = path_.substr(query_start + 1);
      path_ = path_.substr(0, query_start);
    }
  }
};

struct Client {
  std::string buildReq(const std::string& method, URL url) {
    std::stringstream req;
    req << method << " " << url.path_ << " HTTP/1.1\r\n";
    req << "Accept: */*\r\n";
    req << "Connection: keep-alive\r\n";
    req << "Host: " << url.host_ << "\r\n";
    req << "User-Agent: WFS/0.1.0\r\n\r\n";
    return req.str();
  }

  Response request(std::string method, std::string url_str) {
    URL url(url_str);
    Connection c(url.host_, url.port_);
    c.sendRequest(buildReq(method, url));
    Response res = c.recvHeader();
    if (method != "HEAD") {
      c.recvContent(res);
    }
    return res;
  }

  Response get(std::string url_str) {
    return request("GET", url_str);
  }

  Response head(std::string url_str) {
    return request("HEAD", url_str);
  }

  // Async
  template <typename F>
  std::future<void> request(std::string method, std::string url_str, F callback) {
    return std::async(std::launch::async, [=] {
      Response res = request(method, url_str);
      callback(res);
    });
  }

  template <typename F>
  std::future<void> get(std::string url_str, F callback) {
    return request("GET", url_str, callback);
  }

  template <typename F>
  std::future<void> head(std::string url_str, F callback) {
    return request("HEAD", url_str, callback);
  }
};

}  // namespace http
