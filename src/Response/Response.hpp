#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "../Utils/Utils.hpp"

class Response {
	private:
		int			status_code;
		std::string	status_message;
		std::string	body;
		std::map<std::string, std::string> headers;
		bool		close_after_send;

	protected:
		std::string statusMessageCode(int code) const;

	public:
		Response();
		Response(const Response &other);
		Response &operator=(const Response &other);
		~Response();

		void setStatus(int code); //code for error
		void setHeader(const std::string &key, const std::string &value); //header of the error
		void setBody(const std::string &b); //body of the error
		bool setBodyFromFile(const std::string &path); // if we will read a file for make a bdoy
		void setCloseAfterSend(bool close); // if we will close or not

		std::string toString() const; // tranfor object response in a HTTP string with all informations
		
		int getStatus() const; //return the status for we see log internal

		const std::map<std::string, std::string>& getHeaders() const; //return the map of headers
};


#endif /* RESPONSE */