
Roberto (Input / Config / Sockets): lê argumentos, parse/config, valida, cria sockets bind()+listen(), configura non-blocking e adiciona a event-loop / accept loop. Também envia FDs/handles para a Pessoa 2.

Luis (Request parser / reader): faz read() nos FDs (aceitos), monta headers, valida framing (Host obrigatório, Content-Length / chunked), aplica limites (client_max_body_size) e produz um Request pronto.

Rafael (Request handler / CGI / responder): recebe Request, escolhe ServerConfig/Location, executa CGI ou lê ficheiro estático, gera Response (headers + body), gerencia Connection: close/keep-alive e passa o Response para escrita (Pessoa 1 faz o envio final se preferires centralizar I/O).


Roberto -> Luis: entrega ConnectionHandle (contendo fd, listen_key, client addr) — pode ser por fila thread-safe.

Luis -> Rafael: entrega Request (headers map, method, path, body buffer or stream handle).

Rafael -> Roberto (writer): entrega Response (headers, body stream or buffer, close_after_send flag).
