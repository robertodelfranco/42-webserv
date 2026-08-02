#include "ErrorResponse.hpp"

ErrorResponse::ErrorResponse(int code, const std::string &custom_page){
	setStatus(code);
	if (!custom_page.empty() && setBodyFromFile(custom_page)) {
	
	}else{
		std::ostringstream ss;
		ss	<< "<html><head><title>"
			<< code << " " << statusMessageCode(code)
			<< "</title></head><body>";

		ss << "<h1>" << code << " " << statusMessageCode(code) << "</h1>";
        ss << "<p>Server generated error page</p>";
        ss << "</body></html>";

		setBody(ss.str());
	}
	setCloseAfterSend(true);
}