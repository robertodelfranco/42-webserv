#include "ErrorResponse.hpp"

#include <sstream>

#include "ResponseHelpers.hpp"

ErrorResponse::ErrorResponse(int code, const std::string &custom_page){
	setStatus(code);
	std::string body;
	if (!custom_page.empty() && ResponseHelpers::readFileToString(custom_page, body)) {
	} else {
		std::ostringstream ss;
		ss	<< "<html><head><title>"
			<< code << " " << statusMessageCode(code)
			<< "</title></head><body>";

		ss << "<h1>" << code << " " << statusMessageCode(code) << "</h1>";
        ss << "<p>Server generated error page</p>";
        ss << "</body></html>";

		body = ss.str();
	}
	setBody(body);
	setCloseAfterSend(true);
}