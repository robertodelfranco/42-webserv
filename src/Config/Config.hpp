#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <vector>
#include "../ServerConfig/ServerConfig.hpp"

// Fachada pública do módulo de config: orquestra ConfigLexer (texto ->
// tokens) e ConfigParser (tokens -> ServerConfig), e guarda o resultado final.
class Config {
	private:
		std::vector<ServerConfig>	servers;

	public:
		Config();
		~Config();

		void	load(const char *file);

		const std::vector<ServerConfig>&	getServers() const;
};

#endif
