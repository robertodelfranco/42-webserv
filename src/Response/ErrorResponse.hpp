#ifndef ERROR_RESPONSE_HPP
#define ERROR_RESPONSE_HPP

#include "../Utils/Utils.hpp"

class ErrorResponse : public Response {
	public:
		ErrorResponse(int code, const std::string &custom_page = std::string());
};

#endif /* ERROR_RESPONSE_HPP */