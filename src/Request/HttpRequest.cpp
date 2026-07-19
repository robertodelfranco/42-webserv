/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2026/07/18 20:37:36 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

// =======================
// Canonical form
// =======================

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest::HttpRequest()
: raw_(),
  method_(),
  path_(),
  httpVersion_(),
  headers_(),
  body_()
{}

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest::HttpRequest(const HttpRequest &other)
: raw_(other.raw_),
  method_(other.method_),
  path_(other.path_),
  httpVersion_(other.httpVersion_),
  headers_(other.headers_),
  body_(other.body_)
{}

/* não precisaria, mas está aqui para seguir a forma canônica. */
HttpRequest &HttpRequest::operator=(const HttpRequest &other)
{
    if (this != &other) {
        raw_         = other.raw_;
        method_      = other.method_;
        path_        = other.path_;
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

const std::string &HttpRequest::getRaw() const
{
    return raw_;
}

const std::string &HttpRequest::getMethod() const
{
    return method_;
}

const std::string &HttpRequest::getPath() const
{
    return path_;
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

/////// fim ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*                                                                             
/*------------------------------------------------------------------------------
                                     ██                          ▄▄     
████▄ ▄█▀█▄ ▄████ ██ ██ ▄█▀█▄ ▄█▀▀▀ ▀██▀▀   ███▄███▄ ▄███▄ ▄████ ██ ▄█▀ 
██ ▀▀ ██▄█▀ ██ ██ ██ ██ ██▄█▀ ▀███▄  ██     ██ ██ ██ ██ ██ ██    ████   
██    ▀█▄▄▄ ▀████ ▀██▀█ ▀█▄▄▄ ▄▄▄█▀  ██     ██ ██ ██ ▀███▀ ▀████ ██ ▀█▄ 
               ██                                                       
               ▀▀                                                       
APAGAR TUDO ISSO QUANDO CONCLUIR O CONFIG!!!!!!!!!!!! ------------------------*/

HttpRequest::HttpRequest(const std::string &mock)
: raw_(), method_(), path_(), httpVersion_("HTTP/1.1"), headers_(), body_()
{
    if (mock == "MOCK1") {
        method_ = "GET";
        path_   = "/index.html";
    }
    else if (mock == "MOCK2") {
        method_ = "GET";
        path_   = "/uploads/";
    }
    else if (mock == "MOCK3") {
        method_ = "GET";
        path_   = "/uploads";
    }
    else if (mock == "MOCK4") {
        method_ = "POST";
        path_   = "/uploads/novo.txt";
        body_   = "conteudo do arquivo enviado";
        headers_["Content-Type"]   = "text/plain";
        headers_["Content-Length"] = "27";
    }
    else if (mock == "MOCK5") {
        method_ = "DELETE";
        path_   = "/uploads/novo.txt";
    }
    else
        exit(1);
}
