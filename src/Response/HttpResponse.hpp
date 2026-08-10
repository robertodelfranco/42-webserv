/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rafaelheringer <rafaelheringer@student.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/26 10:17:35 by eduribei          #+#    #+#             */
/*   Updated: 2026/08/10 12:03:28 by rafaelherin      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

# include <map>
# include <string>

enum StatusCode
{
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    BAD_REQUEST = 400,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    PAYLOAD_TOO_LARGE = 413,
    INTERNAL_SERVER_ERROR = 500,
};

class HttpResponse
{
    private:
        int						status_code_;
        std::string				status_message_;
        std::string				body_;
        std::map<std::string, std::string> headers_;
        bool					close_after_send_;

    protected:
        std::string statusMessageCode(int code) const;

    public:
        HttpResponse();
        HttpResponse(const HttpResponse &other);
        HttpResponse &operator=(const HttpResponse &other);
        virtual ~HttpResponse();

        void setStatus(int code);
        void setHeader(const std::string &key, const std::string &value);
        void setBody(const std::string &body);
        void setCloseAfterSend(bool close);

        int getStatus() const;
        const std::string &getStatusMessage() const;
        const std::string &getBody() const;
        bool getCloseAfterSend() const;
        const std::map<std::string, std::string> &getHeaders() const;
};

#endif
