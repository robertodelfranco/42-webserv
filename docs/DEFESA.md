# Roteiro de defesa — webserv

Cola de comandos para a avaliação. Cada seção corresponde a um bloco da régua.
Tudo é copiar e colar.

## 0. Preparação (1 minuto)

```bash
# gera a config e as páginas de demonstração num tmp
./tests/tests.sh --emit /tmp/demo

# sobe o servidor
./webserv /tmp/demo/test.conf
```

A config sobe **dois sites**: `localhost:8090` (site A, o grosso dos testes) e
`localhost:8091` (site B, prova de multi-porta).

Se quiser deixar a variável pronta e digitar menos:

```bash
A=http://localhost:8090
B=http://localhost:8091
```

### O atalho: rodar tudo de uma vez

```bash
./tests/tests.sh              # 61 checagens
./tests/tests.sh siege        # + stress test
./tests/tests.sh valgrind     # + relatório de leak
./tests/tests.sh --host <ip>  # contra o servidor na nuvem
```

Ofereça isso ao avaliador **depois** de mostrar os curls à mão — primeiro ele
quer ver você operando o servidor, não um script.

---

## 1. Perguntas de código (antes de qualquer curl)

O avaliador começa perguntando. Onde apontar:

| Pergunta dele | Onde no código | Resposta curta |
|---|---|---|
| Qual mecanismo de evento? | `src/Network/EventLoop.cpp` | `poll()`, um só |
| Um único poll? Mostre o caminho | `EventLoop::run()` → `buildPollfds()` → `handleConnectionEvent()` | um `poll()` no laço principal; o array tem listeners + clientes + pipes de CGI |
| Ele vigia leitura E escrita? | `buildPollfds()` | `POLLIN` se `wantsRead()`, `POLLOUT` se `hasPendingWrite()`, no **mesmo** `pollfd` |
| Como trata accept e read/write? | `acceptReadyListener()` / `Connection::onReadable()` / `onWritable()` | I/O só depois do `revents` indicar prontidão |
| Onde estão os read/recv/write/send? | `Connection.cpp` | `recv` em `onReadable`/`onCgiClientEvent`, `send` em `onWritable` |
| Trata `-1` e `0`? | `Connection::onReadable()` e `onWritable()` | os dois casos são checados **separadamente**, cada um com seu log |
| Usa `errno` depois deles? | — | **Não.** `Connection.cpp` não inclui nem `<cerrno>` |

```bash
# prove que Connection.cpp não tem errno (o arquivo com todos os recv/send)
grep -c errno src/Network/Connection.cpp        # 0

# os únicos errno do projeto são após poll() e accept(), que não são read/write
grep -rn errno src/
```

```bash
# compila limpo, sem relinking
make re
make            # deve dizer "already compiled", sem recompilar nada
```

---

## 2. Configuração

### Múltiplos sites em portas diferentes

```bash
curl -i $A/          # SITE A index
curl -i $B/          # SITE B na outra porta
```

### Página de erro 404 customizada

```bash
curl -i $A/nao_existe        # 404 + "404 CUSTOMIZADO"

# mostre a config e edite o arquivo pra provar que é a página do disco
cat /tmp/demo/site_a/errors/404.html
echo '<h1>MUDEI AO VIVO</h1>' > /tmp/demo/site_a/errors/404.html
curl -s $A/nao_existe                            # agora mostra MUDEI AO VIVO
```

### Limite de tamanho do body (o curl exato da régua)

```bash
# ABAIXO do limite (200 bytes) -> 201 Created
curl -i -X POST -H "Content-Type: plain/text" \
     --data "BODY IS HERE write something shorter" $A/upload/curto.txt

# ACIMA do limite -> 413
curl -i -X POST -H "Content-Type: plain/text" \
     --data "$(python3 -c 'print("X"*300)')" $A/upload/longo.txt
```

### Rotas para diretórios diferentes

```bash
curl -s $A/           | head -3    # vem de site_a/
curl -s $A/listagem/  | head -5    # vem de listagem/
```

### Arquivo default ao pedir um diretório

```bash
curl -i $A/            # serve index.html sem você pedir o nome
```

### Métodos aceitos por rota

```bash
curl -i $A/                       # GET permitido   -> 200
curl -i -X POST $A/               # POST barrado    -> 405
curl -i -X DELETE $A/             # DELETE barrado  -> 405
curl -i -X DELETE $A/upload/x.txt # DELETE liberado nessa rota
```

---

## 3. Basic checks (GET / POST / DELETE)

### O ciclo completo de upload

```bash
curl -i -X POST --data-binary "conteudo do arquivo" $A/upload/demo.txt   # 201
curl -i $A/upload/demo.txt                                              # 200 + conteúdo
curl -i -X DELETE $A/upload/demo.txt                                    # 204
curl -i $A/upload/demo.txt                                              # 404
```

### Métodos desconhecidos não derrubam o servidor

```bash
curl -i -X FOOBAR  $A/     # 501 Not Implemented
curl -i -X PUT     $A/     # 501
curl -i -X OPTIONS $A/     # 501
curl -i $A/                # 200 — o servidor continua vivo
```

### Com telnet (a régua pede telnet explicitamente)

```bash
telnet localhost 8090
GET / HTTP/1.1
Host: x

```
*(linha em branco no final envia a request)*

Versão sem telnet interativo:

```bash
printf 'GET / HTTP/1.1\r\nHost: x\r\n\r\n' | nc localhost 8090

# versão HTTP inválida -> 505
printf 'GET / HTTP/9.9\r\nHost: x\r\n\r\n' | nc localhost 8090

# request malformada -> 400, sem crash
printf 'GET\r\n\r\n' | nc localhost 8090
```

---

## 4. CGI

```bash
curl -i $A/cgi/hello.py                       # GET  -> 200
curl -i -d 'hello world' $A/cgi/echo.py       # POST -> 200, ecoa o body
curl -s $A/cgi/env.py                         # mostra as variáveis CGI
```

### Rodando no diretório correto

```bash
curl -s $A/cgi/env.py | grep -iE "PWD|SCRIPT_FILENAME|PATH_TRANSLATED"
```
O filho faz `chdir` para a pasta do script antes do `execve`, por isso caminho
relativo funciona lá dentro.

### Erros no CGI (a régua exige testar)

```bash
curl -i $A/cgi/crash.py      # script que aborta      -> 502
curl -i $A/cgi/noheader.py   # saída sem header       -> 502
curl -i $A/cgi/big.py        # 5 MB de saída          -> 200
time curl -i $A/cgi/sleep.py # loop infinito          -> 504 em ~20s
curl -i $A/                  # servidor continua vivo -> 200
```

Ponto forte para narrar: **nenhum processo zumbi**. O `CgiProcess` é dono do
filho; se o cliente desistir no meio, a conexão morre e o destrutor do
`CgiProcess` manda `kill` + `waitpid`.

```bash
# conte os filhos do CGI antes, durante e depois de o cliente desistir
cnt() { pgrep -f 'python3.*sleep\.py' | wc -l; }

cnt                                    # 0
curl -s -m 2 $A/cgi/sleep.py >/dev/null &
sleep 1.5; cnt                         # 1  (o filho está rodando)
sleep 4;   cnt                         # 0  (cliente desistiu, filho foi morto)
ps -eo stat= | grep -c '^Z'            # 0  (nenhum zumbi)
```

> Use `pgrep -f`, não `ps -ef | grep`: o `grep` casa com a sua própria linha de
> comando e conta processos que não existem.

### Body chunked é desmontado antes de ir pro CGI

```bash
curl -i -H "Transfer-Encoding: chunked" -d 'hello world' $A/cgi/echo.py
```

---

## 5. Navegador

Abra `http://localhost:8090/` com a aba de rede aberta e mostre:

| O que ele pede | Onde |
|---|---|
| Request e response headers | aba Network, qualquer request |
| Site estático completo | `http://localhost:8090/` |
| URL incorreta | `http://localhost:8090/nao_existe` → 404 customizado |
| Listar diretório | `http://localhost:8090/listagem/` → autoindex |
| URL redirecionada | `http://localhost:8090/old` → 301 para example.com |

Headers pela linha de comando, se ele preferir:

```bash
curl -i -D - -o /dev/null $A/          # response headers
curl -v $A/old 2>&1 | grep -i location # Location do redirect
```

Detalhe que impressiona: `GET /listagem` (sem barra) responde **301** para
`/listagem/`. É o que o nginx faz — sem isso os links relativos da listagem
quebrariam no browser.

---

## 6. Port issues

```bash
# dois sites, duas portas
curl -s $A/ | head -1
curl -s $B/ | head -1
```

### Mesma interface:porta duas vezes → erro limpo

```bash
cat > /tmp/dup.conf <<'EOF'
server {
    listen 8090;
    root /tmp/demo/site_a;
    location / {
        methods GET;
    }
}
server {
    listen 8090;
    root /tmp/demo/site_b;
    location / {
        methods GET;
    }
}
EOF
./webserv /tmp/dup.conf
```

Saída esperada, e o processo sai com código 1 sem crashar:

```
[ERROR] Duplicated Listen without support for virtual host
```

> Atenção ao digitar na hora: o parser exige blocos **multi-linha**.
> `location / { methods GET; }` numa linha só dá erro de sintaxe.

**Não implementamos virtual host** — o subject diz explicitamente que está fora
de escopo. Duas portas iguais dão erro de bind, que é o comportamento válido.

### Dois webserv ao mesmo tempo na mesma porta

```bash
./webserv /tmp/demo/test.conf &     # primeiro sobe
./webserv /tmp/demo/test.conf       # segundo falha no bind e sai
```
O segundo falha porque não usamos `SO_REUSEPORT` — só `SO_REUSEADDR`, que
permite reusar a porta em `TIME_WAIT` mas não dois donos simultâneos.

---

## 7. Siege e memória

```bash
# a régua pede -b; ajuste -c e -r combinando com o avaliador
siege -b -c 25 -r 40 http://localhost:8090/
```

Alvo: **availability acima de 99,5%**. O nosso dá 100%.

Enquanto roda, mostre em outro terminal que a memória não cresce:

```bash
watch -n1 "ps -o pid,rss,etime -p \$(pgrep -f 'webserv /tmp/demo')"
```

Sem conexões penduradas: o `CONNECTION_TIMEOUT` (90s) fecha cliente ocioso e o
`CGI_TIMEOUT` (20s) corta CGI travado.

---

## 8. Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
         ./webserv /tmp/demo/test.conf
```

Rode os testes contra ele e depois `Ctrl+C` para fechar o relatório. Resultado
esperado (já verificado):

```
in use at exit: 0 bytes in 0 blocks
total heap usage: 4.951 allocs, 4.951 frees
All heap blocks were freed -- no leaks are possible
FILE DESCRIPTORS: 3 open (3 std) at exit
ERROR SUMMARY: 0 errors from 0 contexts
```

O `Ctrl+C` sai limpo porque `SIGINT` é tratado: o handler só marca uma flag, o
laço termina a volta e os destrutores rodam (por isso o valgrind fecha zerado).

Ou automatizado:

```bash
./tests/tests.sh valgrind     # roda tudo e imprime o relatório no fim
```

---

## 9. Rodando contra a nuvem

```bash
# 1) gerar as fixtures localmente
./tests/tests.sh --emit /srv/wstest

# 2) subir pro servidor (mesmo caminho absoluto nos dois lados)
rsync -a /srv/wstest/ user@host:/srv/wstest/

# 3) no servidor, sob valgrind, pra você ver os logs ao vivo
valgrind --leak-check=full --track-fds=yes --log-file=valgrind.log \
         ./webserv /srv/wstest/test.conf

# 4) da sua máquina
./tests/tests.sh --host <ip-do-servidor>
```

---

## 10. Perguntas difíceis e as respostas

**"Por que o `/kapouet` rooteado em `/tmp/www` busca em `/tmp/www/pouic/...`?"**
Porque é o que o subject define. O `resolveUriPath` remove o prefixo do location
e junta com o root — semântica de `alias` do nginx, não de `root`.

**"O que acontece se eu mandar duas requests coladas num pacote só?"**
As duas são respondidas. O parser sabe onde a primeira termina (`requestEnd()`),
e o resto vai pro `_readBuffer` da próxima volta.

**"E se o corpo chunked vier malformado?"**
400. O `scanChunked` caminha pelos tamanhos declarados; ele não procura o
terminador com `find`, justamente porque os bytes `\r\n0\r\n\r\n` podem aparecer
**dentro** do dado de um chunk e cortariam a request no lugar errado.

**"Por que `Content-Length` e `Transfer-Encoding` juntos dão 400?"**
Request smuggling. Um proxy na frente pode acreditar num header e nós no outro,
e o que sobra vira uma segunda request injetada. RFC 9112 §6.1 manda recusar.

**"Como você impede path traversal?"**
No `HttpParser::setPath`, em duas etapas e **nesta ordem**: decodifica `%XX` e só
então normaliza. Invertido, `%2e%2e` escaparia da normalização e voltaria a ser
`..` na hora de tocar o disco. `..` que sobe acima da raiz → 403; escape
percentual inválido ou `%00` → 400.

**"Por que HEAD dá 405 se você diz que implementa HEAD?"**
Porque o location da demo só lista `GET`. HEAD tem bit próprio no `methods`, não
é herdado do GET. Num location que liste `HEAD`, ele responde 200 com
`Content-Length` e corpo vazio.

**"Cadê o `unlink` na lista de funções permitidas?"**
Não está — e DELETE é obrigatório. A lista do subject não traz nenhuma função de
remoção, então usamos a primitiva POSIX mínima. Se preferir, dá pra discutir.
