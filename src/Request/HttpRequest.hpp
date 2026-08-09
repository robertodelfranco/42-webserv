/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:00 by eduribei          #+#    #+#             */
/*   Updated: 2026/07/25 17:24:18 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <string>
# include <map>

class HttpRequest
{
	/* assim o HttpParser poder preencher o HttpRequest (acessando os privados)
	sem precisar de um paredão de setters que no fim só vão ser usados por um
	tipo de classe. o custo de usar friend aqui é furar o encapsulamento, e as
	duas classes acopladas podem quebrar uma a outra. por outro lado, o manual
	de estilo do google para C++ cita o nosso caso como o único que justifica
	o uso de friend: uma classe sendo builder da outra. (edu) */
    friend class HttpParser;

    private:
        // ===== Request line =====
		/* para o CGI, talvez path precise ser dividido em path e query_string,
		mas posso resolver isso depois - aqui ou diretamente no CGI. (edu) */
        std::string method_;
        std::string path_;
        std::string httpVersion_;

        // ===== Headers + body =====
        std::map<std::string, std::string> headers_;
        std::string                        body_;

    public:
        // ===== Canonical form =====
		/* o HttpRequest precisa de um construtor padrão bem resolvido porque
		vai ser instanciado por um objeto Client, para só então ser passado por
		referência para o HttpParser preencher. no webserv, não somos obrigados
		a usar OCF, mas é bom ter para refletir caso a caso. Para o HttpRequest,
		além de não usarmos no programa nem copy nem assignment, a classe só tem
		como atributos os containers <string> e <map>. Ambos têm o default copy
		constructor e o default copy assignment bem definidos como deep copy. */
        HttpRequest();
        HttpRequest(const HttpRequest &other);
        HttpRequest &operator=(const HttpRequest &other);
        ~HttpRequest();

        // ===== Public API =====
        const std::string &getMethod() const;
        const std::string &getPath() const;
        const std::string &getHTTPVersion() const;
        const std::string &getBody() const;
        std::string        getHeader(const std::string &key) const;
        const std::map<std::string, std::string> &getHeaders() const;

        // APAGAR QUANDO O PARSER FOR LIGADO
        void    setRequestLineProvisional(std::string& method, std::string& path, std::string& version);


    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    /*--------------------------------------------------------------------------
                                        ██                          ▄▄     
    ████▄ ▄█▀█▄ ▄████ ██ ██ ▄█▀█▄ ▄█▀▀▀ ▀██▀▀   ███▄███▄ ▄███▄ ▄████ ██ ▄█▀ 
    ██ ▀▀ ██▄█▀ ██ ██ ██ ██ ██▄█▀ ▀███▄  ██     ██ ██ ██ ██ ██ ██    ████   
    ██    ▀█▄▄▄ ▀████ ▀██▀█ ▀█▄▄▄ ▄▄▄█▀  ██     ██ ██ ██ ▀███▀ ▀████ ██ ▀█▄ 
                ██                                                       
                ▀▀                                                       */
    /* APAGAR DEPOIS!!! esse aqui é um overload do construtor de HttpRequest
    só para gerar um objeto de HttpRequest com dados mockados para testes. */
    HttpRequest(const std::string &mock);
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////



    };

#endif
