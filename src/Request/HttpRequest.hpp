/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: luide-ca <luide-ca@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:00 by luide-ca          #+#    #+#             */
/*   Updated: 2025/11/25 16:25:21 by luide-ca         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <string>
# include <map>

class HttpRequest
{
    // HttpParser is the only entity allowed to populate a request.
    friend class HttpParser;

    private:
        // ===== Raw data =====
        std::string _raw;

        // ===== Request line =====
        std::string _method;
        std::string _path;
        std::string _httpVersion;

        // ===== Headers + body =====
        std::map<std::string, std::string> _headers;
        std::string                        _body;

    public:
        // ===== Canonical form =====
        HttpRequest();
        HttpRequest(const HttpRequest &other);
        HttpRequest &operator=(const HttpRequest &other);
        ~HttpRequest();

        // ===== Public API =====
        const std::string &getRaw() const;
        const std::string &getMethod() const;
        const std::string &getPath() const;
        const std::string &getHTTPVersion() const;
        const std::map<std::string, std::string> &getHeaders() const;
        std::string        getHeader(const std::string &key) const;
        const std::string &getBody() const;
};

#endif