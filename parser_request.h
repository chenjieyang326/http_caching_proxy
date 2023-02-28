#include <iostream>
#include <string>
#include <unordered_map>

class Parser_request {
public:
  std::string request_content;
  std::string first_line;
  std::string method;
  std::string url;
  std::string hostname;
  std::string port;
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  Parser_request(const std::string &request) : request_content(request) {
    // Find the position of the first newline character in the request
    std::size_t newlinePos = request.find("\r\n");

    // Extract the first line of the request
    first_line = request.substr(0, newlinePos);
    std::cout << "Request First Line is: " << first_line << std::endl;
    // Find the position of the space character after the method in the first
    // line
    std::size_t spacePos = first_line.find(' ');

    // Extract the method from the first line
    method = first_line.substr(0, spacePos);
    std::cout << "Request Method is: " << method << std::endl;
    // Find the position of the space character after the URL in the first line
    std::size_t secondSpacePos = first_line.find(' ', spacePos + 1);

    // Extract the URL from the first line
    url = first_line.substr(spacePos + 1, secondSpacePos - spacePos - 1);
    std::cout << "Request url is: " << url << std::endl;
    std::size_t hostStart = url.find("://") + 3;
    std::size_t hostEnd = url.find(":", hostStart);
    if (hostEnd == std::string::npos) {
      hostEnd = url.find("/", hostStart);
      if (hostEnd == std::string::npos) {
        hostEnd = url.length();
      }
      if (url.substr(0, 5) == "https")
        port = "443";
      else
        port = "80";
    } else {
      // port = url.substr(hostEnd + 1);
      size_t end = hostEnd + 1;
      while (url[end] >= '0' && url[end] <= '9') {
        end++;
      }
      port = url.substr(hostEnd, end - hostEnd);
    }
    hostname = url.substr(hostStart, hostEnd - hostStart);

    // Parse the headers and store them in the headers field
    if (method != "POST" && method != "GET" && method != "CONNECT") return;
    if (method != "POST") {
      std::size_t pos = newlinePos + 1;
      headers["Content-Length"] = "-1";
      while (pos < request.length() - 1) {
        std::size_t endlinePos = request.find("\r\n", pos);
        std::size_t colonPos = request.find(':', pos);
        if (colonPos != std::string::npos) {
          std::string headerName = request.substr(pos, colonPos - pos);
          std::string headerValue =
              request.substr(colonPos + 2, endlinePos - colonPos - 3);
          headers[headerName] = headerValue;
        }
        pos = endlinePos + 1;
      }
      std::cout << "parse request success" << std::endl;
    }
    else { // is post
        std::size_t body_start = request.find("\r\n\r\n") + 4;
        body = request.substr(body_start, request.size() - body_start);
        std::string tmp = "Content-Length";
        std::size_t start = request.find("Content-Length") + tmp.size() + 2;
        std::size_t len = request.find("\r\n") - request.find(" ");
        std::string content_len = request.substr(start, len);
        headers[tmp] = content_len;
    }
  }
};
