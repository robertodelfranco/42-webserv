#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "HttpResponse.hpp"

class Response : public HttpResponse {
	public:
		Response();
		Response(const Response &other);
		Response &operator=(const Response &other);
		virtual ~Response();

		std::string toString() const;
};


#endif /* RESPONSE */