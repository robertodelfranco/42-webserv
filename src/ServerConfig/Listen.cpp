#include "Listen.hpp"

Listen::Listen() : host(), port(0) {}

Listen::Listen(const std::string& host, int port)
: host(host), port(port) {}
