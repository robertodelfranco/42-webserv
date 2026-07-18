/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: luide-ca <luide-ca@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2025/11/25 16:24:33 by luide-ca         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

// =======================
// Canonical form
// =======================

HttpRequest::HttpRequest()
: _raw(),
  _method(),
  _path(),
  _httpVersion(),
  _headers(),
  _body()
{}

HttpRequest::HttpRequest(const HttpRequest &other)
: _raw(other._raw),
  _method(other._method),
  _path(other._path),
  _httpVersion(other._httpVersion),
  _headers(other._headers),
  _body(other._body)
{}

HttpRequest &HttpRequest::operator=(const HttpRequest &other)
{
    if (this != &other) {
        _raw         = other._raw;
        _method      = other._method;
        _path        = other._path;
        _httpVersion = other._httpVersion;
        _headers     = other._headers;
        _body        = other._body;
    }
    return *this;
}

HttpRequest::~HttpRequest()
{}

// =======================
// Public API
// =======================

const std::string &HttpRequest::getRaw() const
{
    return _raw;
}

const std::string &HttpRequest::getMethod() const
{
    return _method;
}

const std::string &HttpRequest::getPath() const
{
    return _path;
}

const std::string &HttpRequest::getHTTPVersion() const
{
    return _httpVersion;
}

const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
    return _headers;
}

std::string HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it == _headers.end())
        return std::string();
    return it->second;
}

const std::string &HttpRequest::getBody() const
{
    return _body;
}