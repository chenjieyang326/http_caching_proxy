#include "proxy.h"

int main() {
  const char * port = "12346";
  Proxy * proxy = new Proxy(port);
  proxy->run();
  return 1;
}