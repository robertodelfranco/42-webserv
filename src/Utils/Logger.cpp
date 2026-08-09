#include "Logger.hpp"
#include "../Config/Color.hpp"
#include <iostream>
#include <ctime>

Logger::Level	Logger::_level		= Logger::INFO;
bool			Logger::_color		= true;
std::ofstream	Logger::_file;

Logger::Stream::Stream(Level level)
	: _level(level), _buffer(), _active(Logger::isEnabled(level)) {}

Logger::Stream::Stream(const Stream& other)
	: _level(other._level), _buffer(), _active(other._active)
{
	if (_active)
		_buffer << other._buffer.str();
	other._active = false;
}

Logger::Stream::~Stream()
{
	if (_active)
		Logger::log(_level, _buffer.str());
}

Logger::Stream&	Logger::Stream::operator<<(std::ostream& (*manip)(std::ostream&))
{
	if (_active)
		manip(_buffer);
	return (*this);
}

Logger::Stream	Logger::debug()
{
	return (Stream(DEBUG));
}

Logger::Stream	Logger::info()
{
	return (Stream(INFO));
}

Logger::Stream	Logger::warning()
{
	return (Stream(WARNING));
}

Logger::Stream	Logger::error()
{
	return (Stream(ERROR));
}

void	Logger::setLevel(Level level)
{
	_level = level;
}

Logger::Level	Logger::getLevel()
{
	return (_level);
}

bool	Logger::setLevelFromString(const std::string& name)
{
	if (name == "debug" || name == "DEBUG")
		_level = DEBUG;
	else if (name == "info" || name == "INFO")
		_level = INFO;
	else if (name == "warning" || name == "WARNING")
		_level = WARNING;
	else if (name == "error" || name == "ERROR")
		_level = ERROR;
	else if (name == "silent" || name == "SILENT")
		_level = SILENT;
	else
		return (false);
	return (true);
}

void	Logger::setColorEnabled(bool enabled)
{
	_color = enabled;
}

bool	Logger::openFile(const std::string& path)
{
	closeFile();
	_file.open(path.c_str(), std::ios::out | std::ios::app);
	return (_file.is_open());
}

void	Logger::closeFile()
{
	if (_file.is_open())
		_file.close();
}

bool	Logger::isEnabled(Level level)
{
	return (level >= _level && _level != SILENT);
}

const char*	Logger::levelName(Level level)
{
	switch (level)
	{
		case DEBUG:		return ("DEBUG  ");
		case INFO:		return ("INFO   ");
		case WARNING:	return ("WARNING");
		case ERROR:		return ("ERROR  ");
		default:		return ("       ");
	}
}

const std::string&	Logger::levelColor(Level level)
{
	switch (level)
	{
		case DEBUG:		return (Color::YELLOW);
		case INFO:		return (Color::GREEN);
		case WARNING:	return (Color::CYAN);
		case ERROR:		return (Color::RED);
		default:		return (Color::RESET);
	}
}

void	Logger::blank(Level level)
{
	if (!isEnabled(level))
		return ;

	std::ostream&	out = (level >= WARNING) ? std::cerr : std::cout;

	out << std::endl;
	if (_file.is_open())
		_file << std::endl;
}

void	Logger::log(Level level, const std::string& message)
{
	if (!isEnabled(level))
		return ;

	std::ostream&	out = (level >= WARNING) ? std::cerr : std::cout;
	std::string		prefix;

	prefix += "[";
	prefix += levelName(level);
	prefix += "] ";

	if (_color)
		out << levelColor(level) << prefix << Color::RESET << message << std::endl;
	else
		out << prefix << message << std::endl;

	if (_file.is_open())
		_file << prefix << message << std::endl;
}
