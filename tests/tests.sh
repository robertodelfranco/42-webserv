#!/usr/bin/env bash
# ===========================================================================
#  tests.sh  —  bateria de regressão do webserv, alinhada à defesa.
#
#  Cada seção abaixo corresponde a um bloco de testes. O script sobe o
#  servidor com uma config própria (gerada na hora, em $FIX), roda os testes,
#  e no fim imprime um placar. Sai com código != 0 se algo falhar.
#
#  Uso:   ./tests/tests.sh          # tudo, menos o siege
#         ./tests/tests.sh siege    # inclui o stress test (precisa de siege)
#
#  Não edita nada do repo: a config e as páginas de teste vivem num tmp.
# ===========================================================================

set -u
cd "$(dirname "$0")/.." || exit 1
ROOT="$(pwd)"
BIN="$ROOT/webserv"
FIX="$(mktemp -d)"
PIDS=""

PASS=0; FAIL=0; FAILED_NAMES=""

cleanup() {
	for p in $PIDS; do kill "$p" 2>/dev/null; done
	rm -rf "$FIX"
}
trap cleanup EXIT

say()  { printf '\n\033[1;36m== %s ==\033[0m\n' "$1"; }
ok()   { PASS=$((PASS+1)); printf '  \033[32mOK\033[0m  %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES\n    - $1"; printf '  \033[31mXX\033[0m  %s\n' "$1"; }

# check_status NOME PORTA CAMINHO ESPERADO [args extra do curl...]
check_status() {
	local name="$1" port="$2" path="$3" want="$4"; shift 4
	local got
	got=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "$@" "http://localhost:$port$path")
	if [ "$got" = "$want" ]; then ok "$name (=$want)"; else bad "$name (esperava $want, veio $got)"; fi
}

# check_body NOME PORTA CAMINHO SUBSTRING [args extra do curl...]
check_body() {
	local name="$1" port="$2" path="$3" needle="$4"; shift 4
	if curl -s --max-time 5 "$@" "http://localhost:$port$path" | grep -q -- "$needle"; then
		ok "$name"; else bad "$name (nao achei '$needle')"; fi
}

# helper python: interpreta os \r\n do shell (senao a request vai malformada)
PYRAW='
import socket, sys, re
port=int(sys.argv[1])
req=sys.argv[2].encode().decode("unicode_escape").encode("latin-1")
s=socket.create_connection(("localhost",port)); s.settimeout(5)
s.sendall(req); d=b""
try:
    while True:
        c=s.recv(65536)
        if not c: break
        d+=c
except socket.timeout: pass
s.close()
'

# raw_status NOME PORTA RAWREQUEST ESPERADO  (bytes crus, pra framing)
raw_status() {
	local name="$1" port="$2" req="$3" want="$4"
	local got
	got=$(python3 -c "$PYRAW"'
m=re.search(rb"HTTP/1\.1 (\d{3})", d)
print(m.group(1).decode() if m else "000")
' "$port" "$req")
	if [ "$got" = "$want" ]; then ok "$name (=$want)"; else bad "$name (esperava $want, veio $got)"; fi
}

# responses_on_conn PORTA REQ -> imprime quantas respostas voltaram (req + GET /)
responses_on_conn() {
	local port="$1" req="$2"
	python3 -c "$PYRAW"'
print(len(re.findall(rb"HTTP/1\.1 \d{3}", d)))
' "$port" "${req}GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"
}

wait_port() {
	local port="$1" i=0
	while [ $i -lt 50 ]; do
		if python3 -c "import socket; socket.create_connection(('localhost',$port),1).close()" 2>/dev/null; then
			return 0; fi
		sleep 0.1; i=$((i+1))
	done
	return 1
}

# --------------------------------------------------------------------------
#  Fixtures: páginas + config, geradas em $FIX
#  (error_page é RELATIVO ao root, senao o joinPath concatena errado)
# --------------------------------------------------------------------------
mkdir -p "$FIX/site_a/errors" "$FIX/site_b" "$FIX/listagem" "$FIX/uploads"
echo '<h1>SITE A index</h1>'            > "$FIX/site_a/index.html"
echo '<h1>404 CUSTOMIZADO</h1>'         > "$FIX/site_a/errors/404.html"
echo '<h1>SITE B na outra porta</h1>'   > "$FIX/site_b/index.html"
echo 'conteudo do arquivo'              > "$FIX/listagem/arquivo.txt"

CGIBIN="$ROOT/root/www/cgi-bin"

cat > "$FIX/test.conf" <<EOF
# Site A (porta 8090) — o grosso dos testes
server {
    listen 8090;
    root $FIX/site_a;
    client_max_body_size 200;
    error_page 404 ./errors/404.html;

    location / {
        root $FIX/site_a;
        methods GET;
        index index.html;
    }
    location /listagem {
        root $FIX/listagem;
        methods GET;
        autoindex on;
    }
    location /old {
        return 301 http://example.com/novo;
    }
    location /upload {
        root $FIX/uploads;
        methods GET POST DELETE;
        upload_path $FIX/uploads;
    }
    location /cgi {
        root $CGIBIN;
        methods GET POST;
        cgi_type .py;
        cgi_path /usr/bin/python3;
    }
}

# Site B (porta 8091) — prova de multi-porta / multi-site
server {
    listen 8091;
    root $FIX/site_b;
    location / {
        root $FIX/site_b;
        methods GET;
        index index.html;
    }
}
EOF

# --------------------------------------------------------------------------
say "Build"
if make -C "$ROOT" >/dev/null 2>&1; then ok "compila limpo"; else bad "COMPILACAO FALHOU"; echo; exit 1; fi

"$BIN" "$FIX/test.conf" >"$FIX/server.log" 2>&1 &
PIDS="$PIDS $!"
if wait_port 8090 && wait_port 8091; then ok "servidor no ar (8090 + 8091)"; else bad "servidor nao subiu"; cat "$FIX/server.log"; exit 1; fi

# --------------------------------------------------------------------------
say "Configuração"
check_body   "index servido ao pedir /"          8090 /              "SITE A index"
check_status "error page custom 404"             8090 /nao_existe 404
check_body   "404 usa a pagina configurada"      8090 /nao_existe   "404 CUSTOMIZADO"
check_status "body dentro do limite (200)"       8090 /upload/a.txt 201 -X POST --data-binary "$(python3 -c 'print("a"*150,end="")')"
check_status "body acima do limite (413)"        8090 /upload/b.txt 413 -X POST --data-binary "$(python3 -c 'print("a"*300,end="")')"
check_status "metodo permitido (GET em /)"       8090 /              200
check_status "metodo barrado (POST em /)"        8090 /              405 -X POST
check_status "DELETE sem permissao (/ so GET)"   8090 /              405 -X DELETE

# --------------------------------------------------------------------------
say "Métodos e status codes"
check_status "POST cria arquivo"      8090 /upload/ciclo.txt 201 -X POST --data-binary "conteudo"
check_body   "GET recupera arquivo"   8090 /upload/ciclo.txt "conteudo"
check_status "DELETE remove"          8090 /upload/ciclo.txt 204 -X DELETE
check_status "GET no removido -> 404" 8090 /upload/ciclo.txt 404
raw_status   "metodo UNKNOWN (nao crasha)"  8090 "FOOBAR / HTTP/1.1\r\nHost: x\r\n\r\n" "501"
raw_status   "PUT -> 501"                   8090 "PUT / HTTP/1.1\r\nHost: x\r\n\r\n"    "501"
raw_status   "versao HTTP invalida -> 505"  8090 "GET / HTTP/9.9\r\nHost: x\r\n\r\n"    "505"

# --------------------------------------------------------------------------
say "Segurança de path (traversal)"
raw_status "traversal cru -> 403"        8090 "GET /../../../../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n" "403"
raw_status "traversal %2e%2e -> 403"     8090 "GET /%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n" "403"
raw_status "escape invalido %zz -> 400"  8090 "GET /%zz HTTP/1.1\r\nHost: x\r\n\r\n" "400"
if curl -s --path-as-is --max-time 5 "http://localhost:8090/../../../../../../etc/passwd" | grep -q "root:x:0:0"; then
	bad "VAZOU /etc/passwd"; else ok "nenhum vazamento de /etc/passwd"; fi

# --------------------------------------------------------------------------
say "Framing do body (chunked / smuggling)"
H="POST /cgi/echo.py HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
raw_status "chunked valido -> 200"             8090 "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n" "200"
raw_status "CL + TE juntos (smuggling) -> 400" 8090 "${H}Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n" "400"
raw_status "TE nao-chunked -> 501"             8090 "${H}Transfer-Encoding: gzip\r\n\r\n" "501"
raw_status "chunked malformado -> 400"         8090 "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\nZZZ\r\nx\r\n0\r\n\r\n" "400"
raw_status "Content-Length com lixo -> 400"    8090 "${H}Content-Length: 5abc\r\n\r\nhello" "400"

# --------------------------------------------------------------------------
say "Keep-alive e pipelining"
n=$(responses_on_conn 8090 "GET / HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 2 ] && ok "keep-alive mantem a conexao" || bad "keep-alive quebrou (veio $n)"
n=$(responses_on_conn 8090 "GET /nao_existe HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 2 ] && ok "404 NAO fecha a conexao" || bad "404 fechou a conexao (veio $n)"
n=$(responses_on_conn 8090 "GET / HTTP/1.1\r\nHost: x\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 3 ] && ok "pipelining (3 requests coladas)" || bad "pipelining perdeu request (veio $n)"

# --------------------------------------------------------------------------
say "Diretório e redirect"
check_status "autoindex sem barra -> 301"    8090 /listagem 301
check_status "autoindex com barra -> 200"    8090 /listagem/ 200
check_body   "listagem mostra o arquivo"     8090 /listagem/ "arquivo.txt"
check_status "redirect /old -> 301"          8090 /old 301
loc=$(curl -s -o /dev/null -w '%{redirect_url}' --max-time 5 "http://localhost:8090/old")
[ "$loc" = "http://example.com/novo" ] && ok "redirect com Location correto" || bad "Location errado ($loc)"

# --------------------------------------------------------------------------
say "CGI"
check_status "CGI GET"              8090 /cgi/hello.py 200
check_status "CGI POST"            8090 /cgi/echo.py  200 -d 'hello world'
check_body   "CGI POST ecoa body"  8090 /cgi/echo.py "hello world" -d 'hello world'
check_status "CGI crash -> 502"    8090 /cgi/crash.py 502
check_status "CGI sem header -> 502" 8090 /cgi/noheader.py 502
t0=$(date +%s)
code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 40 "http://localhost:8090/cgi/sleep.py")
t1=$(date +%s)
if [ "$code" = "504" ] && [ $((t1-t0)) -lt 35 ]; then
	ok "CGI timeout -> 504 em $((t1-t0))s (nao pendura)"
else bad "CGI timeout falhou (code=$code em $((t1-t0))s)"; fi

# --------------------------------------------------------------------------
say "Multi-porta / multi-site"
check_body "porta 8090 serve SITE A"  8090 / "SITE A index"
check_body "porta 8091 serve SITE B"  8091 / "SITE B na outra porta"

cat > "$FIX/dup.conf" <<EOF
server { listen 8090; root $FIX/site_a; location / { methods GET; } }
server { listen 8090; root $FIX/site_b; location / { methods GET; } }
EOF
"$BIN" "$FIX/dup.conf" >"$FIX/dup.log" 2>&1 &
duppid=$!; sleep 1
if kill -0 "$duppid" 2>/dev/null; then
	bad "porta duplicada devia falhar no bind (mas subiu)"; kill "$duppid" 2>/dev/null
else
	ok "porta duplicada -> falha limpa de bind (sem crash)"; fi

# --------------------------------------------------------------------------
say "Estabilidade / sem crash"
# saraivada de lixo: nenhum pode derrubar o servidor
for junk in '\r\n\r\n' 'GET\r\n\r\n' 'GET / HTTP/1.1\r\n\r\n' 'AAAAAAAAAAAAAAAAAAAAAAAA\r\n\r\n'; do
	python3 -c "$PYRAW" 8090 "$junk" >/dev/null 2>&1
done
check_status "servidor vivo apos a saraivada de lixo" 8090 / 200

# --------------------------------------------------------------------------
#  Siege (opcional)
# --------------------------------------------------------------------------
if [ "${1:-}" = "siege" ]; then
	say "Siege (stress + availability + leak)"
	if ! command -v siege >/dev/null; then
		bad "siege nao instalado (brew install siege)"
	else
		srvpid=$(pgrep -f "$FIX/test.conf" | head -1)
		rss0=$(ps -o rss= -p "$srvpid" 2>/dev/null | tr -d ' ')
		echo "  rodando siege -b -c 25 -r 40 (1000 hits)..."
		out=$(siege -b -c 25 -r 40 "http://localhost:8090/" 2>&1)
		avail=$(echo "$out" | grep -i availab | grep -oE '[0-9.]+' | head -1)
		rss1=$(ps -o rss= -p "$srvpid" 2>/dev/null | tr -d ' ')
		echo "$out" | grep -iE "availab|transactions|failed|longest"
		if [ -n "$avail" ] && awk "BEGIN{exit !($avail >= 99.5)}"; then
			ok "disponibilidade $avail% (>= 99.5%)"; else bad "disponibilidade ${avail:-?}% (< 99.5%)"; fi
		echo "  RSS antes=${rss0}kB depois=${rss1}kB"
		if [ -n "$rss0" ] && [ -n "$rss1" ] && awk "BEGIN{exit !($rss1 <= $rss0*1.5)}"; then
			ok "RSS estavel"; else bad "RSS cresceu demais ($rss0 -> $rss1)"; fi
	fi
fi

# --------------------------------------------------------------------------
say "Placar"
printf '  \033[32m%d passaram\033[0m, \033[31m%d falharam\033[0m\n' "$PASS" "$FAIL"
if [ "$FAIL" -gt 0 ]; then printf '  falhas:%b\n' "$FAILED_NAMES"; exit 1; fi
echo "  tudo verde."
