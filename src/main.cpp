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
        config.load(av[1]);
        config.getServers(); // just to test if it works, we don't do anything with the result
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
