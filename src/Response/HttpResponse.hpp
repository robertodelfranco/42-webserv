/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/26 10:17:35 by eduribei          #+#    #+#             */
/*   Updated: 2026/07/26 10:17:37 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

class HttpResponse
{
	
    public:
        HttpResponse();
        HttpResponse(const HttpResponse &other);
        HttpResponse &operator=(const HttpResponse &other);
        ~HttpResponse();
    };

#endif
