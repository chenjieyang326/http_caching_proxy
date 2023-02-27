#include <string>
#include <unordered_map>
#include <ctime>
#include <sstream>
#include <iostream>

class Response_parser {
public:
    std::string response_content;
    std::string status;
    std::string date;
    std::string ContentType;
    std::string Expires;
    int maxAge;
    std::string TransferEncoding;
    std::string header;
    std::string body;
    int content_len;
    std::string CacheControl;
    std::string Etag;
    std::string LastModified;
    std::string firstLine;
    time_t convertedDate;
    time_t convertedExpires;

    Response_parser(const std::string& response) : response_content(response) {
        // Split response into headers and body
        size_t pos = response.find("\r\n\r\n");
        header = response.substr(0, pos);
        body = response.substr(pos + 4);

        // Parse status line
        pos = header.find("\r\n");
        firstLine = header.substr(0, pos);
        header = header.substr(pos + 2);
        parseFirstLine();

        // Parse headers
        parseHeaders();
    }

private:
    void parseFirstLine() {
        size_t pos1 = firstLine.find(' ');
        size_t pos2 = firstLine.find(' ', pos1 + 1);
        status = firstLine.substr(pos1 + 1, pos2 - pos1 - 1);
    }

    void parseHeaders() {
        std::unordered_map<std::string, std::string> headers_map;
        std::string line;
        std::istringstream header_stream(header);
        headers_map["Content-Length"] = "-1";
        while (std::getline(header_stream, line)) {
            if (line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }
            size_t pos = line.find(':');
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            headers_map[key] = value;
        }

        // Assign values to fields
        ContentType = headers_map["Content-Type"];
        CacheControl = headers_map["Cache-Control"];
        Etag = headers_map["ETag"];
        LastModified = headers_map["Last-Modified"];
        TransferEncoding = headers_map["Transfer-Encoding"];
        Expires = headers_map["Expires"];
        content_len = std::stoi(headers_map["Content-Length"]);

        // Convert date and expires to time_t
        date = headers_map["Date"];
        struct tm tm;
        strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        convertedDate = mktime(&tm);
        if (headers_map.count("Expires")) {
            Expires = headers_map["Expires"];
            strptime(Expires.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
            convertedExpires = mktime(&tm);
            maxAge = difftime(convertedExpires, convertedDate);
        } else {
            convertedExpires = -1;
            maxAge = -1;
        }
    }
};
