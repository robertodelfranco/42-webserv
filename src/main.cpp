#include "Network/EventLoop.hpp"
#include "Config/Config.hpp"
#include "Utils/Logger.hpp"
#include <signal.h>
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
	signal(SIGPIPE, SIG_IGN);

	// nivel default é INFO, WEBSERV_LOG_LEVEL=debug religa o debug sem
	// precisar de flag (o subject só aceita ./webserv [config])
	const char*	levelEnv = std::getenv("WEBSERV_LOG_LEVEL");
	if (!levelEnv || !Logger::setLevelFromString(levelEnv))
		Logger::setLevel(Logger::INFO);

	if (ac > 2) {
		std::cout << "Usage: ./webserv [Configuration File]" << std::endl;
		return 1;
	}
	// TESTING COMMIT
	Config config;
	try {
		const char* path = resolveConfigPath(ac, av);
		Logger::info() << "Lendo config: " << path << (ac == 2 ? "" : " config/default.conf");
		config.load(resolveConfigPath(ac, av));
		EventLoop	loop(config.getServers());
		loop.run();
	}
	catch (const std::exception& e) {
		Logger::error() << e.what();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
