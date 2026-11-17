#ifndef CPPHTTPLIB_HTTPLIB_H
#define CPPHTTPLIB_HTTPLIB_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <algorithm>

// Fallback for MinGW without gthreads / std::mutex support
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
namespace std {
    class mutex {
        CRITICAL_SECTION cs;
    public:
        mutex() { InitializeCriticalSection(&cs); }
        ~mutex() { DeleteCriticalSection(&cs); }
        void lock() { EnterCriticalSection(&cs); }
        void unlock() { LeaveCriticalSection(&cs); }
    private:
        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;
    };
}
#endif

namespace httplib {

namespace detail {
    inline std::string to_lower(std::string s) {
        for (size_t i = 0; i < s.length(); ++i) {
            s[i] = (char)::tolower((unsigned char)s[i]);
        }
        return s;
    }

    inline std::string url_decode(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '+') {
                out += ' ';
            } else if (in[i] == '%' && i + 2 < in.size()) {
                char h1 = in[i + 1];
                char h2 = in[i + 2];
                int v1 = (h1 >= '0' && h1 <= '9') ? (h1 - '0') :
                         (h1 >= 'a' && h1 <= 'f') ? (h1 - 'a' + 10) :
                         (h1 >= 'A' && h1 <= 'F') ? (h1 - 'A' + 10) : -1;
                int v2 = (h2 >= '0' && h2 <= '9') ? (h2 - '0') :
                         (h2 >= 'a' && h2 <= 'f') ? (h2 - 'a' + 10) :
                         (h2 >= 'A' && h2 <= 'F') ? (h2 - 'A' + 10) : -1;
                if (v1 != -1 && v2 != -1) {
                    out += static_cast<char>((v1 << 4) | v2);
                    i += 2;
                } else {
                    out += in[i];
                }
            } else {
                out += in[i];
            }
        }
        return out;
    }

    inline void parse_params(const std::string& str, std::map<std::string, std::string>& params) {
        size_t start = 0;
        while (start < str.size()) {
            size_t amp = str.find('&', start);
            if (amp == std::string::npos) amp = str.size();
            size_t eq = str.find('=', start);
            if (eq < amp) {
                std::string k = url_decode(str.substr(start, eq - start));
                std::string v = url_decode(str.substr(eq + 1, amp - eq - 1));
                params[k] = v;
            } else {
                std::string k = url_decode(str.substr(start, amp - start));
                if (!k.empty()) params[k] = "";
            }
            start = amp + 1;
        }
    }

    inline std::string get_mime_type(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = to_lower(path.substr(dot));
            if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
            if (ext == ".css") return "text/css; charset=utf-8";
            if (ext == ".js") return "application/javascript; charset=utf-8";
            if (ext == ".json") return "application/json; charset=utf-8";
            if (ext == ".png") return "image/png";
            if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
            if (ext == ".gif") return "image/gif";
            if (ext == ".svg") return "image/svg+xml";
            if (ext == ".ico") return "image/x-icon";
            if (ext == ".txt") return "text/plain; charset=utf-8";
        }
        return "application/octet-stream";
    }

    inline bool dir_exists(const std::string& dir) {
#ifdef _WIN32
        DWORD attribs = GetFileAttributesA(dir.c_str());
        return (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY));
#else
        return true;
#endif
    }
} // namespace detail

struct Request {
    std::string method;
    std::string path;
    std::string raw_path;
    std::string query_string;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::smatch matches;

    std::string get_header_value(const std::string& key) const {
        std::string target = detail::to_lower(key);
        for (const auto& h : headers) {
            if (detail::to_lower(h.first) == target) {
                return h.second;
            }
        }
        return "";
    }

    std::string get_param_value(const std::string& key) const {
        auto it = params.find(key);
        if (it != params.end()) {
            return it->second;
        }
        return "";
    }
};

struct Response {
    int status = 200;
    std::map<std::string, std::string> headers;
    std::string body;

    void set_header(const std::string& key, const std::string& val) {
        headers[key] = val;
    }

    void set_content(const std::string& content, const std::string& content_type) {
        body = content;
        set_header("Content-Type", content_type);
    }
};

class Server {
public:
    using Handler = std::function<void(const Request&, Response&)>;

private:
    struct Route {
        std::string method;
        std::string pattern;
        bool is_regex;
        Handler handler;
    };

    std::vector<Route> routes_;
    std::string mount_mount_;
    std::string mount_dir_;

    void add_route(const std::string& method, const std::string& pattern, Handler handler) {
        bool is_regex = (pattern.find('(') != std::string::npos ||
                         pattern.find('\\') != std::string::npos ||
                         pattern.find('*') != std::string::npos);
        routes_.push_back({ method, pattern, is_regex, handler });
    }

#ifdef _WIN32
    struct ClientContext {
        Server* server;
        SOCKET sock;
    };

    static DWORD WINAPI ClientThreadProc(LPVOID lpParam) {
        ClientContext* ctx = static_cast<ClientContext*>(lpParam);
        ctx->server->handle_client(ctx->sock);
        delete ctx;
        return 0;
    }
#endif

public:
    Server() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    ~Server() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Get(const std::string& pattern, Handler handler) {
        add_route("GET", pattern, handler);
    }

    void Post(const std::string& pattern, Handler handler) {
        add_route("POST", pattern, handler);
    }

    bool set_mount_point(const std::string& mount, const std::string& dir) {
        if (!detail::dir_exists(dir)) {
            return false;
        }
        mount_mount_ = mount;
        mount_dir_ = dir;
        return true;
    }

    void handle_client(SOCKET sock) {
        std::string raw_req;
        char buf[4096];
        size_t header_end = std::string::npos;

        while (header_end == std::string::npos) {
            int n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                closesocket(sock);
                return;
            }
            raw_req.append(buf, n);
            header_end = raw_req.find("\r\n\r\n");
            if (raw_req.size() > 1048576) {
                closesocket(sock);
                return;
            }
        }

        std::string header_part = raw_req.substr(0, header_end);
        std::string body_part = raw_req.substr(header_end + 4);

        Request req;
        std::istringstream stream(header_part);
        std::string line;

        if (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream rl(line);
            rl >> req.method >> req.raw_path;
        }

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string k = line.substr(0, colon);
                std::string v = line.substr(colon + 1);
                while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
                while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
                req.headers[k] = v;
            }
        }

        size_t content_length = 0;
        std::string cl_str = req.get_header_value("Content-Length");
        if (!cl_str.empty()) {
            content_length = (size_t)std::strtoul(cl_str.c_str(), NULL, 10);
        }

        while (body_part.size() < content_length) {
            int to_read = (int)std::min((size_t)sizeof(buf), content_length - body_part.size());
            int n = recv(sock, buf, to_read, 0);
            if (n <= 0) break;
            body_part.append(buf, n);
        }
        req.body = body_part;

        // Path and query parameters
        size_t qmark = req.raw_path.find('?');
        if (qmark != std::string::npos) {
            req.path = req.raw_path.substr(0, qmark);
            req.query_string = req.raw_path.substr(qmark + 1);
            detail::parse_params(req.query_string, req.params);
        } else {
            req.path = req.raw_path;
        }

        // Form parameters from POST body
        if (req.method == "POST") {
            std::string ct = req.get_header_value("Content-Type");
            if (ct.find("application/x-www-form-urlencoded") != std::string::npos || ct.empty()) {
                detail::parse_params(req.body, req.params);
            }
        }

        Response res;

        // CORS preflight OPTIONS
        if (req.method == "OPTIONS") {
            res.status = 200;
            res.set_content("", "text/plain");
            send_response(sock, res);
            return;
        }

        // Match route
        bool matched = false;
        for (const auto& route : routes_) {
            if (route.method != req.method) continue;
            if (!route.is_regex) {
                if (req.path == route.pattern) {
                    matched = true;
                    route.handler(req, res);
                    break;
                }
            } else {
                std::regex re(route.pattern);
                if (std::regex_match(req.path, req.matches, re)) {
                    matched = true;
                    route.handler(req, res);
                    break;
                }
            }
        }

        // If not matched, try static file mount
        if (!matched && !mount_dir_.empty() && (req.method == "GET" || req.method == "HEAD")) {
            std::string subpath = req.path;
            if (subpath == "/" || subpath.empty()) {
                subpath = "/index.html";
            }
            if (subpath.find("..") == std::string::npos) {
                std::string full_path = mount_dir_ + subpath;
                std::ifstream f(full_path.c_str(), std::ios::binary);
                if (f) {
                    res.body.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    res.status = 200;
                    res.set_header("Content-Type", detail::get_mime_type(full_path));
                    matched = true;
                }
            }
        }

        if (!matched) {
            res.status = 404;
            res.set_content("{\"error\":\"Not Found\"}", "application/json");
        }

        send_response(sock, res);
    }

    void send_response(SOCKET sock, const Response& res) {
        std::string status_text = "OK";
        if (res.status == 200) status_text = "OK";
        else if (res.status == 201) status_text = "Created";
        else if (res.status == 204) status_text = "No Content";
        else if (res.status == 400) status_text = "Bad Request";
        else if (res.status == 401) status_text = "Unauthorized";
        else if (res.status == 403) status_text = "Forbidden";
        else if (res.status == 404) status_text = "Not Found";
        else if (res.status == 500) status_text = "Internal Server Error";

        std::ostringstream oss;
        oss << "HTTP/1.1 " << res.status << " " << status_text << "\r\n";
        bool has_ct = false;
        for (const auto& h : res.headers) {
            if (detail::to_lower(h.first) == "content-type") has_ct = true;
            oss << h.first << ": " << h.second << "\r\n";
        }
        if (!has_ct) {
            oss << "Content-Type: text/plain; charset=utf-8\r\n";
        }
        oss << "Content-Length: " << res.body.size() << "\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: *\r\n";
        oss << "Connection: close\r\n\r\n";
        oss << res.body;

        std::string s = oss.str();
        send(sock, s.data(), (int)s.size(), 0);
        closesocket(sock);
    }

    bool listen(const std::string& host, int port) {
        SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_sock == INVALID_SOCKET) {
            return false;
        }

        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((u_short)port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (bind(server_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(server_sock);
            return false;
        }

        if (::listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(server_sock);
            return false;
        }

        while (true) {
            SOCKET client_sock = accept(server_sock, NULL, NULL);
            if (client_sock == INVALID_SOCKET) {
                break;
            }

            DWORD timeout = 5000;
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

#ifdef _WIN32
            ClientContext* ctx = new ClientContext{ this, client_sock };
            HANDLE hThread = CreateThread(NULL, 0, ClientThreadProc, ctx, 0, NULL);
            if (hThread) {
                CloseHandle(hThread);
            } else {
                handle_client(client_sock);
                delete ctx;
            }
#else
            handle_client(client_sock);
#endif
        }

        closesocket(server_sock);
        return true;
    }
};

} // namespace httplib

#endif // CPPHTTPLIB_HTTPLIB_H
