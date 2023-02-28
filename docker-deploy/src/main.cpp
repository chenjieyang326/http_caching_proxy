#include "proxy.h"

int main() {
  const char * port = "12345";
  Proxy * proxy = new Proxy(port);
  proxy->run();
  return 1;
}