#ifndef RESPONSE_WRITER_HPP
#define RESPONSE_WRITER_HPP

#include <string>

class HttpResponse;

class ResponseWriter {
	public:
		static std::string write(const HttpResponse &response);
};

#endif /* RESPONSE_WRITER_HPP */