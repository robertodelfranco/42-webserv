*This project has been created as part of the 42 curriculum by rdel-fra, eduribei, rheringe.*

# Webserv

## Description

`webserv` is a non-blocking, single-threaded HTTP server written in C++98 with no external dependencies. The subject suggests HTTP/1.0 as the reference point and deliberately requires only a subset of the HTTP RFCs, so we took HTTP/1.0 as the baseline and layered on the HTTP/1.1 features that make the server more capable: persistent connections (keep-alive), chunked request bodies, and pipelining. Everything is built around a single `poll()` call. One event loop drives all of it: listening sockets, client sockets, and the pipes of CGI child processes all live in the same `pollfd` array, so no single I/O operation can ever block the whole server.

The goal of the project is to reimplement, from scratch, the core of a web server comparable to a minimal nginx: parse HTTP requests, serve static files, run CGI scripts, handle uploads, and stay responsive under load, all while never reading or writing an event-driven descriptor outside of `poll()`.

Configuration uses an nginx-like syntax (`server { ... }`, `location /path { ... }`). What the server does:

- **Methods**: GET, POST, DELETE and HEAD, each with its own bit in the `methods` directive of a location (HEAD is not inferred from GET). A method the server does not implement answers 501.
- **Asynchronous CGI**: the child process is launched and adopted by the connection, which outlives it across many `poll()` cycles. It has its own timeout and output cap, and the child is always reaped, so no zombie is left behind even if the client gives up mid-request.
- **HTTP/1.0 and HTTP/1.1 framing**: the parser accepts both request-line versions. Persistence follows the version's default (HTTP/1.0 closes after the response, HTTP/1.1 stays open), and an explicit `Connection: close` / `Connection: keep-alive` header overrides that default in either direction. Being a deliberate subset, some 1.1-only mechanisms are out of scope (mandatory `Host` rejection, `Expect: 100-continue`).
- **Keep-alive and pipelining**: requests packed into a single packet are processed in sequence; a semantic error (404, 405) does not tear down the connection.
- **Uploads** via POST with `upload_path`, and removal via DELETE.
- **Directory listing** (`autoindex`), with a 301 redirect to the trailing-slash form.
- **Redirects** via `return <code> <url>`.
- **Custom error pages** per server or per location.
- **Body size limit** (`client_max_body_size`), answering 413 when exceeded.
- **Request safety**: percent-decoding and path normalization reject directory traversal (403) and malformed escapes (400); `Content-Length`/`Transfer-Encoding` conflicts and malformed chunked bodies are rejected instead of mis-framed.

Resource management is RAII throughout: `FileDescriptor` guarantees a single `close()`, `Socket` owns its fd, `Connection` owns its request/response/parser and its CGI process, and `EventLoop` owns every socket and connection.

## Instructions

### Build

```bash
make          # incremental build
make re       # clean everything and rebuild
make clean    # remove object files
make fclean   # remove object files and the binary
```

Compiler: `c++` with `-Wall -Werror -Wextra -std=c++98`. The C++98 standard is a hard requirement of the subject, so no C++11 features are used.

### Run

```bash
./webserv [config_file]     # with no argument, uses config/default.conf
```

### Test

There is no unit-test framework; verification is manual, with `curl` and `telnet` against a running server. A regression suite that mirrors the defense checklist lives in `tests/tests.sh`:

```bash
./tests/tests.sh            # full battery (config, methods, CGI, framing, ...)
./tests/tests.sh siege      # also runs the Siege availability/leak stress test
```

Quick manual checks:

```bash
./webserv config/cgi_test.conf
curl -v localhost:8082/cgi/hello.py               # CGI GET
curl -d 'key=value' localhost:8082/cgi/echo.py    # CGI POST

./webserv config/42.conf
curl -X POST -H "Content-Type: text/plain" \
     --data "$(python3 -c 'print("a"*101,end="")')" \
     localhost:8080/post_body                      # expects 413
```

The CGI scripts used by the tests live in `root/www/cgi-bin/`: `hello.py`, `env.py`, `echo.py`, `sleep.py`, `crash.py`, `big.py`, `bigpost.py`, `noheader.py`.

### Configuration

Directives accepted by the parser:

| Directive | Scope | Effect |
|---|---|---|
| `listen` | server | endpoint port or `host:port` |
| `root` | server, location | on-disk root |
| `index` | server, location | index files; the first that exists wins |
| `error_page` | server, location | custom page for a status code |
| `client_max_body_size` | server, location | request body limit |
| `location` | server | route block (longest-prefix match) |
| `methods` | location | allowed methods: `GET POST DELETE HEAD` |
| `return` | location | redirect: `return 301 http://...` |
| `autoindex` | location | `on`/`off`, directory listing |
| `upload_path` | location | where POST writes uploaded files |
| `cgi_type` | location | extension that triggers CGI (e.g. `.py`) |
| `cgi_path` | location | interpreter (e.g. `/usr/bin/python3`) |

A location inherits `root`, `index`, `error_page` and `client_max_body_size` from its server when it does not declare its own. Minimal example:

```nginx
server {
    listen 8080;
    root ./root/www;
    client_max_body_size 1M;
    error_page 404 ./errors/404.html;

    location / {
        methods GET HEAD;
        index index.html;
        autoindex off;
    }
    location /upload {
        methods GET POST DELETE;
        upload_path ./root/uploads;
    }
    location /cgi {
        root ./root/www/cgi-bin;
        methods GET POST;
        cgi_type .py;
        cgi_path /usr/bin/python3;
    }
}
```

## Resources

Classic references used while building the server:

- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 9112: HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112.html): message framing, `Content-Length` vs `Transfer-Encoding`, chunked bodies
- [RFC 1945: HTTP/1.0](https://www.rfc-editor.org/rfc/rfc1945.html): the baseline version suggested by the subject (default `Connection: close`, no chunked request bodies)
- [RFC 3875: CGI/1.1](https://www.rfc-editor.org/rfc/rfc3875.html): the CGI environment variables and the interpreter contract
- [nginx documentation](https://nginx.org/en/docs/): reference for the config syntax and expected behavior in edge cases (redirects, autoindex, error pages)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/): sockets and `poll()`
- `man 2 poll`, `man 2 socket`, `man 2 execve`, `man 2 waitpid`

### Use of AI

AI (Claude) was used as an assistant for **debugging, regression testing, and code review**, never as the author of the architecture. The design (single `poll()` loop, the `HttpParser` state machine with `feed()`/`RequestStatus`, the `Connection` state machine, the ephemeral handler pattern) was decided by the team. Concretely, AI was used to:

- **Hunt and fix bugs** in the request pipeline: a path-traversal hole in GET and DELETE (percent-decode + normalization now reject it), `Content-Length` / `Transfer-Encoding` smuggling, chunked framing that mis-detected the terminator inside chunk data, a lost-request bug in HTTP pipelining, and a 431 path that never fired.
- **Write the regression suite** in `tests/tests.sh`, aligned to the evaluation checklist.
- **Study other webserv implementations**: nine peer projects were mapped into class diagrams (classes, relations, responsibilities), each with notes on its organization and use of OOP, compiled into a single comparison flowchart that we used to contrast architectural choices before settling on ours.
- **Review** the request-parsing and response code for correctness against the relevant RFCs.

Every change proposed by AI was read, validated, and adjusted by the team before being committed; nothing was accepted blindly.