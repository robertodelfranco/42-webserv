/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2026/08/22 14:37:41 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib> // USADO NO MOCK, APAGAR ESSE INCLUDE DEPOIS
#include "HttpRequest.hpp"

// =======================
// Canonical form
// =======================

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest::HttpRequest()
: method_(),
  path_(),
  query_(),
  httpVersion_(),
  headers_(),
  body_()
{}

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest::HttpRequest(const HttpRequest &other)
: method_(other.method_),
  path_(other.path_),
  query_(other.query_),
  httpVersion_(other.httpVersion_),
  headers_(other.headers_),
  body_(other.body_)
{}

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest &HttpRequest::operator=(const HttpRequest &other)
{
    if (this != &other) {
        method_      = other.method_;
        path_        = other.path_;
        query_       = other.query_;
        httpVersion_ = other.httpVersion_;
        headers_     = other.headers_;
        body_        = other.body_;
    }
    return *this;
}

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest::~HttpRequest()
{}

// =======================
// Public API
// =======================

const std::string &HttpRequest::getMethod() const
{
    return method_;
}

const std::string &HttpRequest::getPath() const
{
    return path_;
}

// precisa separar path da query
const std::string &HttpRequest::getQuery() const
{
    return query_;
}

const std::string &HttpRequest::getHTTPVersion() const
{
    return httpVersion_;
}

const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
    return headers_;
}

std::string HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it = headers_.find(key);
    if (it == headers_.end())
        return std::string();
    return it->second;
}

const std::string &HttpRequest::getBody() const
{
    return body_;
}
