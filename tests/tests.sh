#!/usr/bin/env bash
# ===========================================================================
#  tests.sh  —  bateria de regressão do webserv, alinhada ao subject da defesa.
#
#  MODOS
#    ./tests/tests.sh                     local: sobe o servidor e testa tudo
#    ./tests/tests.sh siege               local + stress test (precisa de siege)
#    ./tests/tests.sh valgrind            local sob valgrind, imprime o leak report
#    ./tests/tests.sh --host 1.2.3.4      remoto: testa um servidor já no ar
#    ./tests/tests.sh --emit /srv/wstest  gera config+fixtures pra subir na nuvem
#
#  REMOTO — como usar:
#    1) ./tests/tests.sh --emit /srv/wstest        (gera localmente)
#    2) rsync -a /srv/wstest/ user@host:/srv/wstest/
#       rsync -a root/www/cgi-bin/ user@host:/srv/wstest/cgi-bin/
#    3) no servidor:  ./webserv /srv/wstest/test.conf
#       (ou sob valgrind, veja o cabeçalho do arquivo gerado)
#    4) aqui:  ./tests/tests.sh --host <ip-do-servidor>
#
#  O caminho do --emit precisa ser o MESMO nos dois lados: a config guarda
#  caminhos absolutos, então o diretório tem que existir igual no servidor.
#
#  Não edita nada do repo: em modo local tudo vive num tmp descartável.
# ===========================================================================

set -u
cd "$(dirname "$0")/.." || exit 1
ROOT="$(pwd)"
BIN="$ROOT/webserv"

# 127.0.0.1 e não "localhost": em máquina cujo /etc/hosts só mapeia localhost
# para ::1 (IPv6), o servidor — que faz bind AF_INET — fica inalcançável pelo
# nome, e os testes falhavam com "servidor nao subiu" mesmo ele estando no ar.
HOST="127.0.0.1"
PORT_A=8090
PORT_B=8091
MODE="local"          # local | remote | emit
EXTRA=""              # siege | valgrind
EMIT_DIR=""

while [ $# -gt 0 ]; do
	case "$1" in
		--host)  HOST="$2"; MODE="remote"; shift 2 ;;
		--port)  PORT_A="$2"; shift 2 ;;
		--port-b) PORT_B="$2"; shift 2 ;;
		--emit)  MODE="emit"; EMIT_DIR="$2"; shift 2 ;;
		siege|valgrind) EXTRA="$1"; shift ;;
		*) echo "argumento desconhecido: $1"; exit 2 ;;
	esac
done

PIDS=""
PASS=0; FAIL=0; SKIP=0; FAILED_NAMES=""

if [ "$MODE" = "emit" ]; then FIX="$EMIT_DIR"; else FIX="$(mktemp -d)"; fi

cleanup() {
	for p in $PIDS; do kill "$p" 2>/dev/null; done
	[ "$MODE" = "local" ] && rm -rf "$FIX"
	return 0
}
trap cleanup EXIT

say()  { printf '\n\033[1;36m== %s ==\033[0m\n' "$1"; }
ok()   { PASS=$((PASS+1)); printf '  \033[32mOK\033[0m  %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES\n    - $1"; printf '  \033[31mXX\033[0m  %s\n' "$1"; }
skip() { SKIP=$((SKIP+1)); printf '  \033[33m--\033[0m  %s (pulado: %s)\n' "$1" "$2"; }

# check_status NOME PORTA CAMINHO ESPERADO [args extra do curl...]
check_status() {
	local name="$1" port="$2" path="$3" want="$4"; shift 4
	local got
	got=$(curl -s -o /dev/null -w '%{http_code}' --max-time 10 "$@" "http://$HOST:$port$path")
	if [ "$got" = "$want" ]; then ok "$name (=$want)"; else bad "$name (esperava $want, veio $got)"; fi
}

# check_body NOME PORTA CAMINHO SUBSTRING [args extra do curl...]
check_body() {
	local name="$1" port="$2" path="$3" needle="$4"; shift 4
	if curl -s --max-time 10 "$@" "http://$HOST:$port$path" | grep -q -- "$needle"; then
		ok "$name"; else bad "$name (nao achei '$needle')"; fi
}

# check_header NOME PORTA CAMINHO REGEX-DO-HEADER  (a régua olha headers no browser)
check_header() {
	local name="$1" port="$2" path="$3" rx="$4"; shift 4
	if curl -s -D - -o /dev/null --max-time 10 "$@" "http://$HOST:$port$path" | grep -qiE "$rx"; then
		ok "$name"; else bad "$name (header nao casou /$rx/)"; fi
}

PYRAW='
import socket, sys, re
host=sys.argv[1]; port=int(sys.argv[2])
req=sys.argv[3].encode().decode("unicode_escape").encode("latin-1")
s=socket.create_connection((host,port),10); s.settimeout(10)
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
' "$HOST" "$port" "$req")
	if [ "$got" = "$want" ]; then ok "$name (=$want)"; else bad "$name (esperava $want, veio $got)"; fi
}

responses_on_conn() {
	local port="$1" req="$2"
	python3 -c "$PYRAW"'
print(len(re.findall(rb"HTTP/1\.1 \d{3}", d)))
' "$HOST" "$port" "${req}GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"
}

wait_port() {
	local port="$1" i=0
	while [ $i -lt 80 ]; do
		if python3 -c "import socket; socket.create_connection(('$HOST',$port),1).close()" 2>/dev/null; then
			return 0; fi
		sleep 0.1; i=$((i+1))
	done
	return 1
}

# --------------------------------------------------------------------------
#  Fixtures: páginas + config
#  (error_page é RELATIVO ao root, senão o joinPath concatena errado)
# --------------------------------------------------------------------------
# O bônus de "multiple CGI systems" só pode ser testado onde o php-cgi existe.
# Precisa ser condicional: a validação do cgi_path recusa um interpretador
# inexistente no boot, então incluir a location sem o binário derrubaria a
# config inteira em vez de pular um teste.
# Detecta sozinho, mas o ambiente vence: "PHP_CGI= ./tests/tests.sh" força o
# skip (útil para checar que a suíte roda numa máquina sem PHP).
if [ -z "${PHP_CGI+definido}" ]; then
	PHP_CGI="$(command -v php-cgi 2>/dev/null || true)"
fi

make_fixtures() {
	local base="$1" cgibin="$2"
	mkdir -p "$base/site_a/errors" "$base/site_b" "$base/listagem" "$base/uploads"
	echo '<h1>SITE A index</h1>'          > "$base/site_a/index.html"
	echo '<h1>404 CUSTOMIZADO</h1>'       > "$base/site_a/errors/404.html"
	echo '<h1>SITE B na outra porta</h1>' > "$base/site_b/index.html"
	echo 'conteudo do arquivo'            > "$base/listagem/arquivo.txt"

	cat > "$base/test.conf" <<EOF
# Gerado por tests/tests.sh — config de demonstração da defesa.
# Para rodar sob valgrind:
#   valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \\
#            --log-file=valgrind.log ./webserv $base/test.conf

# Site A — o grosso dos testes
server {
    listen $PORT_A;
    root $base/site_a;
    client_max_body_size 200;
    error_page 404 ./errors/404.html;

    location / {
        root $base/site_a;
        methods GET;
        index index.html;
    }
    location /listagem {
        root $base/listagem;
        methods GET;
        autoindex on;
    }
    location /old {
        return 301 http://example.com/novo;
    }
    location /upload {
        root $base/uploads;
        methods GET POST DELETE;
        upload_path $base/uploads;
    }
    location /cgi {
        root $cgibin;
        methods GET POST;
        cgi_type .py;
        cgi_path /usr/bin/python3;
    }
$([ -n "$PHP_CGI" ] && printf '%s\n' "    location /cgi-php {
        root $cgibin;
        methods GET POST;
        cgi_type .php;
        cgi_path $PHP_CGI;
    }")
}

# Site B — prova de multi-porta / multi-site
server {
    listen $PORT_B;
    root $base/site_b;
    location / {
        root $base/site_b;
        methods GET;
        index index.html;
    }
}
EOF
}

# --------------------------------------------------------------------------
#  Modo --emit: só gera os arquivos e sai
# --------------------------------------------------------------------------
if [ "$MODE" = "emit" ]; then
	make_fixtures "$EMIT_DIR" "$EMIT_DIR/cgi-bin"
	mkdir -p "$EMIT_DIR/cgi-bin"
	cp "$ROOT"/root/www/cgi-bin/*.py "$EMIT_DIR/cgi-bin/" 2>/dev/null
	cp "$ROOT"/root/www/cgi-bin/*.php "$EMIT_DIR/cgi-bin/" 2>/dev/null
	echo "Fixtures geradas em: $EMIT_DIR"
	echo
	echo "  1) rsync -a '$EMIT_DIR/' user@host:'$EMIT_DIR/'"
	echo "  2) no servidor: ./webserv '$EMIT_DIR/test.conf'"
	echo "     sob valgrind: valgrind --leak-check=full --show-leak-kinds=all \\"
	echo "                            --track-fds=yes --log-file=valgrind.log \\"
	echo "                            ./webserv '$EMIT_DIR/test.conf'"
	echo "  3) aqui: ./tests/tests.sh --host <ip>"
	exit 0
fi

# --------------------------------------------------------------------------
#  Sobe o servidor (só em modo local)
# --------------------------------------------------------------------------
if [ "$MODE" = "local" ]; then
	say "Build"
	if make -C "$ROOT" >/dev/null 2>&1; then ok "compila limpo"; else bad "COMPILACAO FALHOU"; exit 1; fi
	# relinking: um segundo make não pode recompilar nada
	if make -C "$ROOT" 2>&1 | grep -qi "already compiled\|Nothing to be done"; then
		ok "sem relinking desnecessario"; else bad "Makefile relinka sem necessidade"; fi

	# ---------------------------------------------------------------------
	#  Todo config versionado precisa SUBIR. Pega coisas que só aparecem ao
	#  executar: diretório que não existe no repo (o git não versiona pasta
	#  vazia), caminho errado, diretiva inválida.
	#  Um de cada vez, porque vários configs usam a mesma porta.
	# ---------------------------------------------------------------------
	say "Configs do repositorio"
	for cfg in "$ROOT"/config/*.conf; do
		[ -f "$cfg" ] || continue
		name=$(basename "$cfg")
		"$BIN" "$cfg" >"$FIX/cfg.log" 2>&1 &
		cpid=$!
		sleep 1
		if kill -0 "$cpid" 2>/dev/null; then
			ok "$name sobe"
		else
			bad "$name NAO sobe: $(grep -i 'error' "$FIX/cfg.log" | head -1 | sed 's/\x1b\[[0-9;]*m//g')"
		fi
		# Vários configs compartilham porta (42.conf e config.conf usam 8080).
		# Esperar o processo morrer de verdade, e não um sleep fixo: com sleep
		# o próximo config tentava bindar antes de a porta ser liberada, e o
		# teste falhava de forma intermitente — que é pior que não testar.
		kill "$cpid" 2>/dev/null
		wait "$cpid" 2>/dev/null
		i=0
		while kill -0 "$cpid" 2>/dev/null && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
	done

	make_fixtures "$FIX" "$ROOT/root/www/cgi-bin"

	if [ "$EXTRA" = "valgrind" ]; then
		if ! command -v valgrind >/dev/null; then echo "valgrind nao instalado"; exit 1; fi
		valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
		         --log-file="$FIX/valgrind.log" "$BIN" "$FIX/test.conf" >"$FIX/server.log" 2>&1 &
	else
		"$BIN" "$FIX/test.conf" >"$FIX/server.log" 2>&1 &
	fi
	PIDS="$PIDS $!"
	if wait_port "$PORT_A" && wait_port "$PORT_B"; then ok "servidor no ar ($PORT_A + $PORT_B)"
	else bad "servidor nao subiu"; cat "$FIX/server.log"; exit 1; fi
else
	say "Modo remoto: $HOST:$PORT_A e $HOST:$PORT_B"
	if wait_port "$PORT_A"; then ok "servidor remoto respondendo em $HOST:$PORT_A"
	else bad "nao alcancei $HOST:$PORT_A"; exit 1; fi
	wait_port "$PORT_B" && ok "servidor remoto respondendo em $HOST:$PORT_B" \
	                   || skip "site B em $HOST:$PORT_B" "porta nao responde"
fi

# --------------------------------------------------------------------------
#  Confere que o alvo roda A CONFIG DESTE SCRIPT antes de testar qualquer coisa.
#
#  A bateria não é agnóstica de config: ela espera as rotas /listagem, /old,
#  /upload e /cgi, mais a página de índice e o 404 customizado. Apontar o
#  script pra um servidor rodando outra config (default.conf, 42.conf, ...)
#  produz uma dúzia de falhas que parecem bug do servidor e não são.
#  Por isso: uma mensagem clara aqui, em vez de 12 confusas lá embaixo.
# --------------------------------------------------------------------------
if ! curl -s --max-time 10 "http://$HOST:$PORT_A/" | grep -q "SITE A index"; then
	printf '\n\033[31m  O servidor em %s:%s NAO esta rodando a config deste script.\033[0m\n' "$HOST" "$PORT_A"
	cat <<EOF

  A bateria espera as rotas /listagem, /old, /upload e /cgi. Gere a config e
  suba o servidor com ELA:

      ./tests/tests.sh --emit /tmp/demo --port $PORT_A --port-b $PORT_B
      ./webserv /tmp/demo/test.conf

  E entao rode:

      ./tests/tests.sh --host $HOST --port $PORT_A --port-b $PORT_B

EOF
	exit 2
fi

# --------------------------------------------------------------------------
say "Configuração (régua: config file)"
check_body   "default file ao pedir um diretorio"  "$PORT_A" /            "SITE A index"
check_status "URL incorreta -> 404"                "$PORT_A" /nao_existe 404
check_body   "error page 404 customizada"          "$PORT_A" /nao_existe "404 CUSTOMIZADO"
check_status "body DENTRO do limite"               "$PORT_A" /upload/a.txt 201 -X POST -H "Content-Type: plain/text" --data "$(python3 -c 'print("a"*150,end="")')"
check_status "body ACIMA do limite -> 413"         "$PORT_A" /upload/b.txt 413 -X POST -H "Content-Type: plain/text" --data "$(python3 -c 'print("a"*300,end="")')"
check_status "rota para outro diretorio"           "$PORT_A" /listagem/ 200
check_status "metodo aceito na rota (GET)"         "$PORT_A" /            200
check_status "metodo NAO aceito na rota (POST)"    "$PORT_A" /            405 -X POST
check_status "DELETE sem permissao -> 405"         "$PORT_A" /            405 -X DELETE

# --------------------------------------------------------------------------
say "Basic checks (régua: GET/POST/DELETE, upload)"
check_status "POST cria arquivo (upload)"  "$PORT_A" /upload/ciclo.txt 201 -X POST --data-binary "conteudo enviado"
check_body   "GET recupera o arquivo"      "$PORT_A" /upload/ciclo.txt "conteudo enviado"
check_status "DELETE com permissao -> 204" "$PORT_A" /upload/ciclo.txt 204 -X DELETE
check_status "GET no removido -> 404"      "$PORT_A" /upload/ciclo.txt 404
raw_status   "metodo UNKNOWN nao crasha"   "$PORT_A" "FOOBAR / HTTP/1.1\r\nHost: x\r\n\r\n" "501"
raw_status   "PUT -> 501"                  "$PORT_A" "PUT / HTTP/1.1\r\nHost: x\r\n\r\n"    "501"
raw_status   "versao HTTP invalida -> 505" "$PORT_A" "GET / HTTP/9.9\r\nHost: x\r\n\r\n"    "505"
raw_status   "request vazia nao crasha"    "$PORT_A" "\r\n\r\n"                             "400"
raw_status   "request line sem alvo"       "$PORT_A" "GET\r\n\r\n"                          "400"

# --------------------------------------------------------------------------
say "Headers (régua: olhar request/response header no browser)"
check_header "Content-Type no estatico"    "$PORT_A" /            "^content-type:.*text/html"
check_header "Content-Length presente"     "$PORT_A" /            "^content-length:"
check_header "Server/status line coerente" "$PORT_A" /            "^HTTP/1\.1 200"
check_header "Location no redirect"        "$PORT_A" /old         "^location:.*example\.com/novo"
check_header "Connection keep-alive"       "$PORT_A" /            "^connection:.*(keep-alive|close)"

# --------------------------------------------------------------------------
say "Segurança de path"
raw_status "traversal cru -> 403"       "$PORT_A" "GET /../../../../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n" "403"
raw_status "traversal %2e%2e -> 403"    "$PORT_A" "GET /%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n" "403"
raw_status "escape invalido %zz -> 400" "$PORT_A" "GET /%zz HTTP/1.1\r\nHost: x\r\n\r\n" "400"
raw_status "byte nulo %00 -> 400"       "$PORT_A" "GET /a%00.png HTTP/1.1\r\nHost: x\r\n\r\n" "400"
if curl -s --path-as-is --max-time 10 "http://$HOST:$PORT_A/../../../../../../etc/passwd" | grep -q "root:x:0:0"; then
	bad "VAZOU /etc/passwd"; else ok "nenhum vazamento de /etc/passwd"; fi

# --------------------------------------------------------------------------
say "Framing do body (chunked / smuggling)"
H="POST /cgi/echo.py HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
raw_status "chunked valido -> 200"             "$PORT_A" "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n" "200"
raw_status "chunked multiplos chunks -> 200"   "$PORT_A" "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n" "200"
raw_status "chunked com trailer -> 200"        "$PORT_A" "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\nX-T: y\r\n\r\n" "200"
raw_status "CL + TE juntos (smuggling) -> 400" "$PORT_A" "${H}Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n" "400"
raw_status "TE nao-chunked -> 501"             "$PORT_A" "${H}Transfer-Encoding: gzip\r\n\r\n" "501"
raw_status "chunked malformado -> 400"         "$PORT_A" "${H}Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\nZZZ\r\nx\r\n0\r\n\r\n" "400"
raw_status "Content-Length com lixo -> 400"    "$PORT_A" "${H}Content-Length: 5abc\r\n\r\nhello" "400"

# O dado do chunk contem literalmente os bytes do terminador ("\r\n0\r\n\r\n").
# Quem procura o fim com find() casa aqui dentro e corta a request no meio:
# o status continua 200, só o BODY vem truncado — por isso o teste olha o
# corpo ecoado, e não o status.
chunk_data='AB\r\n0\r\n\r\nCDE'      # 12 bytes
body=$(python3 -c "$PYRAW"'
i = d.find(b"\r\n\r\n")
print(d[i+4:].decode("latin-1") if i >= 0 else "")
' "$HOST" "$PORT_A" "${H}Transfer-Encoding: chunked\r\n\r\nc\r\n${chunk_data}\r\n0\r\n\r\n")
if echo "$body" | grep -q "len=12"; then
	ok "terminador dentro do dado do chunk (body integro)"
else
	bad "terminador dentro do dado cortou a request (esperava len=12, veio '$(echo "$body" | head -1)')"
fi
raw_status "Content-Length negativo -> 400"    "$PORT_A" "${H}Content-Length: -1\r\n\r\n" "400"

# --------------------------------------------------------------------------
say "Keep-alive e pipelining"
n=$(responses_on_conn "$PORT_A" "GET / HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 2 ] && ok "keep-alive mantem a conexao" || bad "keep-alive quebrou (veio $n)"
n=$(responses_on_conn "$PORT_A" "GET /nao_existe HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 2 ] && ok "404 NAO fecha a conexao" || bad "404 fechou a conexao (veio $n)"
n=$(responses_on_conn "$PORT_A" "GET / HTTP/1.1\r\nHost: x\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n")
[ "$n" -ge 3 ] && ok "pipelining (3 requests coladas)" || bad "pipelining perdeu request (veio $n)"

# --------------------------------------------------------------------------
say "Diretório e redirect (régua: listar dir, URL redirecionada)"
check_status "autoindex sem barra -> 301"  "$PORT_A" /listagem 301
check_status "autoindex com barra -> 200"  "$PORT_A" /listagem/ 200
check_body   "listagem mostra o arquivo"   "$PORT_A" /listagem/ "arquivo.txt"
check_status "redirect /old -> 301"        "$PORT_A" /old 301
loc=$(curl -s -o /dev/null -w '%{redirect_url}' --max-time 10 "http://$HOST:$PORT_A/old")
[ "$loc" = "http://example.com/novo" ] && ok "redirect com Location correto" || bad "Location errado ($loc)"

# --------------------------------------------------------------------------
say "CGI (régua: GET, POST, erros, diretório correto)"
check_status "CGI GET"                 "$PORT_A" /cgi/hello.py 200
check_status "CGI POST"                "$PORT_A" /cgi/echo.py  200 -d 'hello world'
check_body   "CGI POST ecoa o body"    "$PORT_A" /cgi/echo.py "hello world" -d 'hello world'
check_body   "CGI roda no dir correto" "$PORT_A" /cgi/env.py "PWD\|SCRIPT\|CGI"
check_status "CGI com erro -> 502"     "$PORT_A" /cgi/crash.py 502
check_status "CGI sem header -> 502"   "$PORT_A" /cgi/noheader.py 502
check_status "CGI body grande (5MB)"   "$PORT_A" /cgi/big.py 200
t0=$(date +%s)
code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 60 "http://$HOST:$PORT_A/cgi/sleep.py")
t1=$(date +%s)
if [ "$code" = "504" ] && [ $((t1-t0)) -lt 55 ]; then
	ok "CGI loop infinito -> 504 em $((t1-t0))s (nao pendura)"
else bad "CGI timeout falhou (code=$code em $((t1-t0))s)"; fi
check_status "servidor vivo depois do CGI ruim" "$PORT_A" / 200

# Bônus da régua: dois interpretadores diferentes na MESMA config.
if [ -n "$PHP_CGI" ]; then
	check_status "CGI .php GET"           "$PORT_A" "/cgi-php/info.php?nome=teste" 200
	check_body   "CGI .php le a query"    "$PORT_A" "/cgi-php/info.php?nome=teste" "query       : nome=teste"
	check_body   "CGI .php le o body"     "$PORT_A" /cgi-php/info.php "body        : mensagem=oi" -d 'mensagem=oi'
	check_body   "CGI .php roda no dir certo" "$PORT_A" /cgi-php/info.php "cwd         : cgi-bin"
	# o ponto do bônus: .py e .php respondendo no mesmo servidor, mesma config
	check_status ".py ainda responde (2 CGIs juntos)" "$PORT_A" /cgi/hello.py 200
else
	skip "CGI .php (bonus: multiple CGI systems)" "php-cgi nao instalado"
fi

# --------------------------------------------------------------------------
say "Multi-porta / multi-site (régua: port issues)"
check_body "porta $PORT_A serve SITE A"  "$PORT_A" / "SITE A index"
if [ "$MODE" = "local" ]; then
	check_body "porta $PORT_B serve SITE B"  "$PORT_B" / "SITE B na outra porta"
	cat > "$FIX/dup.conf" <<EOF
server { listen $PORT_A; root $FIX/site_a; location / { methods GET; } }
server { listen $PORT_A; root $FIX/site_b; location / { methods GET; } }
EOF
	"$BIN" "$FIX/dup.conf" >"$FIX/dup.log" 2>&1 &
	duppid=$!; sleep 1
	if kill -0 "$duppid" 2>/dev/null; then
		bad "porta duplicada devia falhar no bind (mas subiu)"; kill "$duppid" 2>/dev/null
	else ok "porta duplicada -> falha limpa de bind, sem crash"; fi
else
	check_body "porta $PORT_B serve SITE B"  "$PORT_B" / "SITE B na outra porta"
	skip "bind duplicado" "so faz sentido local"
fi

# --------------------------------------------------------------------------
say "Resiliência (régua: nunca crashar)"
for junk in '\r\n\r\n' 'GET\r\n\r\n' 'GET / HTTP/1.1\r\n\r\n' 'AAAAAAAAAAAAAAAAAAAAAAAA\r\n\r\n' \
            'GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 999999\r\n\r\nshort'; do
	python3 -c "$PYRAW" "$HOST" "$PORT_A" "$junk" >/dev/null 2>&1
done
check_status "servidor vivo apos a saraivada de lixo" "$PORT_A" / 200
# conexão aberta e abandonada não pode pendurar o servidor
python3 -c "
import socket
s=socket.create_connection(('$HOST',$PORT_A),5)
s.sendall(b'GET / HTTP/1.1\r\nHost: x\r\n')   # request incompleta, some
s.close()" 2>/dev/null
check_status "servidor vivo apos conexao abandonada" "$PORT_A" / 200

# --------------------------------------------------------------------------
if [ "$EXTRA" = "siege" ]; then
	say "Siege (régua: availability > 99.5%, sem leak, sem hanging)"
	if ! command -v siege >/dev/null; then
		bad "siege nao instalado (brew install siege)"
	else
		srvpid=""
		[ "$MODE" = "local" ] && srvpid=$(pgrep -f "$FIX/test.conf" | head -1)
		rss0=""; [ -n "$srvpid" ] && rss0=$(ps -o rss= -p "$srvpid" 2>/dev/null | tr -d ' ')
		echo "  siege -b -c 25 -r 40 (1000 hits)..."
		out=$(siege -b -c 25 -r 40 "http://$HOST:$PORT_A/" 2>&1)
		echo "$out" | grep -iE "availab|transactions|failed|longest"
		avail=$(echo "$out" | grep -i availab | grep -oE '[0-9.]+' | head -1)
		if [ -n "$avail" ] && awk "BEGIN{exit !($avail >= 99.5)}"; then
			ok "disponibilidade $avail% (>= 99.5%)"; else bad "disponibilidade ${avail:-?}% (< 99.5%)"; fi
		if [ -n "$srvpid" ]; then
			rss1=$(ps -o rss= -p "$srvpid" 2>/dev/null | tr -d ' ')
			echo "  RSS antes=${rss0}kB depois=${rss1}kB"
			if awk "BEGIN{exit !($rss1 <= $rss0*1.5)}"; then ok "RSS estavel (sem leak visivel)"
			else bad "RSS cresceu demais ($rss0 -> $rss1)"; fi
		else
			skip "medicao de RSS" "servidor e remoto, olhe la"
		fi
	fi
fi

# --------------------------------------------------------------------------
say "Placar"
printf '  \033[32m%d passaram\033[0m, \033[31m%d falharam\033[0m, \033[33m%d pulados\033[0m\n' "$PASS" "$FAIL" "$SKIP"
[ "$FAIL" -gt 0 ] && printf '  falhas:%b\n' "$FAILED_NAMES"

# --------------------------------------------------------------------------
if [ "$EXTRA" = "valgrind" ] && [ "$MODE" = "local" ]; then
	say "Valgrind"
	echo "  encerrando o servidor pra o valgrind fechar o relatorio..."
	for p in $PIDS; do kill -TERM "$p" 2>/dev/null; done
	sleep 3
	if [ -f "$FIX/valgrind.log" ]; then
		cp "$FIX/valgrind.log" "$ROOT/valgrind.log"
		grep -E "in use at exit|total heap usage|All heap blocks|definitely lost|indirectly lost|possibly lost|still reachable|ERROR SUMMARY|FILE DESCRIPTORS" "$ROOT/valgrind.log" | sed 's/^/  /'
		echo "  relatorio completo salvo em: $ROOT/valgrind.log"
	else
		echo "  (nenhum log gerado)"
	fi
fi

[ "$FAIL" -gt 0 ] && exit 1
echo "  tudo verde."
exit 0
