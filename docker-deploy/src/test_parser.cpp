#include "parser_request.h"

#include <cstring>
#include <string>
#include <iostream>
using namespace std;

#include <cstring>
#include "parser_response.h"

void printFields(const Parser_request& p) {
    std::cout << "Method: " << p.method << "\n";
    std::cout << "First line: " << p.first_line << "\n";
    std::cout << "Headers:\n";
    for (const auto& header : p.headers) {
        std::cout << "  " << header.first << ": " << header.second << "\n";
    }
    std::cout << "Port: " << p.port << "\n";
    std::cout << "Hostname: " << p.hostname << "\n";
}

void parseFirstLine(const std::string& firstLine, const char** hostname, const char** port) {
    // Find the position of the first space character in the first line
    std::size_t spacePos = firstLine.find(' ');

    // Find the position of the second space character in the first line
    std::size_t secondSpacePos = firstLine.find(' ', spacePos + 1);

    // Extract the substring between the first and second space characters
    std::string url = firstLine.substr(spacePos + 1, secondSpacePos - spacePos - 1);

    // Find the position of the colon character in the URL
    std::size_t colonPos = url.find(':');

    // If the URL contains a colon character, extract the hostname and port
    if (colonPos != std::string::npos) {
        *hostname = url.substr(0, colonPos).c_str();
        *port = url.substr(colonPos + 1).c_str();
    }
    // Otherwise, assume that the URL contains only the hostname and use the default port 80
    else {
        *hostname = url.c_str();
        *port = "80";
    }
}

void printResponseFields(const Response_parser& parser) {
    std::cout << "Status: " << parser.status << std::endl;
    std::cout << "Date: " << parser.date << std::endl;
    std::cout << "Content-Type: " << parser.ContentType << std::endl;
    std::cout << "Expires: " << parser.Expires << std::endl;
    std::cout << "Max-Age: " << parser.maxAge << std::endl;
    std::cout << "Transfer-Encoding: " << parser.TransferEncoding << std::endl;
    std::cout << "Header: " << parser.header << std::endl;
    std::cout << "Body: " << parser.body << std::endl;
    std::cout << "Content-Length: " << parser.content_len << std::endl;
    std::cout << "Cache-Control: " << parser.CacheControl << std::endl;
    std::cout << "ETag: " << parser.Etag << std::endl;
    std::cout << "Last-Modified: " << parser.LastModified << std::endl;
    std::cout << "First Line: " << parser.firstLine << std::endl;
    std::cout << "Converted Date: " << parser.convertedDate << std::endl;
    std::cout << "Converted Expires: " << parser.convertedExpires << std::endl;
}




// int main() {
// //   Parser_request parser(
// //       "GET /index.html HTTP/1.1\r\nHost: example.com\r\nUser-Agent: "
// //       "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
// //       "like Gecko) Chrome/88.0.4324.150 Safari/537.36\r\nAccept: "
// //       "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/"
// //       "apng,*/*;q=0.8,application/"
// //       "signed-exchange;v=b3;q=0.9\r\nAccept-Encoding: gzip, deflate, "
// //       "br\r\nConnection: keep-alive\r\n");
// //   printFields(parser);
// //   const char *hostname;
// //   const char *port;
// //   string first_line = "GET http://www.example.com:8080/index.html HTTP/1.1\r\n";
// //   parseFirstLine(first_line, &hostname, &port);
// //   cout << first_line << endl;
// //   cout << hostname << endl;
// //   cout << port << endl;
    
// }

int main() {
    // Example HTTP response
    std::string response = "HTTP/1.1 200 OK\r\nDate: Tue, 01 Mar 2022 15:45:56 GMT\r\nContent-Type: text/html; charset=UTF-8\r\nExpires: Tue, 01 Mar 2023 15:45:56 GMT\r\nCache-Control: public, max-age=31536000\r\nETag: \"abc123\"\r\nLast-Modified: Tue, 01 Mar 2022 10:00:00 GMT\r\nContent-Length: 1234\r\n\r\n<html><body><h1>Hello, world!</h1></body></html>";
                           
    // Parse the response
    Response_parser parser(response);
    
    // Print out all fields
    // printResponseFields(parser);
    cout << parser.Etag << endl;
    cout << parser.CacheControl << endl; 
    cout << parser.Expires << endl;
    cout << parser.date << endl;
    return 0;
}
