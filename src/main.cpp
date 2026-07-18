#include "Config/Config.hpp"
#include <iostream>
#include <cstdlib>

int main(int ac, char **av)
{
    if (ac > 2) {
        std::cout << "Usage: ./webserv [Configuration File]" << std::endl;
        return 1;
    }

    Config config;
    try {
        config.initLexer(av[1]);
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
