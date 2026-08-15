#ifndef LOGGER_HPP
# define LOGGER_HPP

#include <string>
#include <sstream>
#include <fstream>

// Logger::info() << "Servidor ouvindo em " << host << ":" << port;
class Logger {
	public:
		// A ordem importa: só é impresso o que tiver nível >= _level.
		enum Level {
			DEBUG	= 0,
			INFO	= 1,
			WARNING	= 2,
			ERROR	= 3,
			SILENT	= 4
		};

		// Temporário devolvido por debug()/info()/warning()/error(): acumula
		// tudo que vier com << e imprime uma única linha quando é destruído
		class Stream {
			public:
				explicit Stream(Level level);
				Stream(const Stream& other);
				~Stream();

				template <typename T>
				Stream&	operator<<(const T& value)
				{
					if (_active)
						_buffer << value;
					return (*this);
				}

				// Aceita manipuladores (std::endl, std::hex) sem quebrar a
				// dedução do template acima.
				Stream&	operator<<(std::ostream& (*manip)(std::ostream&));

			private:
				Stream& operator=(const Stream& other);

				Level				_level;
				std::ostringstream	_buffer;
				mutable bool		_active; // a cópia mais nova é quem imprime
		};

		static Stream	debug();
		static Stream	info();
		static Stream	warning();
		static Stream	error();

		static void		setLevel(Level level);
		static Level	getLevel();
		static bool		setLevelFromString(const std::string& name); // "debug", "info", false se inválido

		static void		setColorEnabled(bool enabled);

		static bool		openFile(const std::string& path); // espelha a saída num arquivo
		static void		closeFile();

		static bool		isEnabled(Level level);
		static void		log(Level level, const std::string& message);
		static void		blank(Level level); // linha em branco pra separar blocos no terminal

	private:
		Logger();
		~Logger();
		Logger(const Logger& other);
		Logger& operator=(const Logger& other);

		static Level			_level;
		static bool				_color;
		static std::ofstream	_file;

		static const char*			levelName(Level level);
		static const std::string&	levelColor(Level level);
};

#endif
