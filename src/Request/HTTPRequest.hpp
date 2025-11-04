/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: luide-ca <luide-ca@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/04 09:58:08 by luide-ca          #+#    #+#             */
/*   Updated: 2025/11/04 15:53:32 by luide-ca         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <iostream>
# include <exception>

class HTTPRequest {
	private:

	public:
			HTTPRequest(void);
			HTTPRequest( /*feel with everything necessary*/ );
			HTTPRequest( const HTTPRequest& other );
			~HTTPRequest( /*feel with everything necessary*/ );
			
			HTTPRequest& operator=( const HTTPRequest& other );
};

#endif