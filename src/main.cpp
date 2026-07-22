#include "Network/EventLoop.hpp"
#include "Config/Config.hpp"
#include <iostream>
#include <cstdlib>

static const char*	resolveConfigPath(int ac, char **av)
{
	if (ac == 2)
		return av[1];
	return "config/default.conf"; // enunciado: cai pra um caminho default se não vier argumento
}

int main(int ac, char **av)
{
	if (ac > 2) {
		std::cout << "Usage: ./webserv [Configuration File]" << std::endl;
		return 1;
	}

	Config config;
	try {
		config.init(resolveConfigPath(ac, av));
		EventLoop	loop(config.getServers());
		loop.run();
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
