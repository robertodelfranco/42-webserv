<?php
// CGI em PHP: prova que cgi_type/cgi_path sao por location, nao globais.
// Roda com php-cgi (o SAPI de CGI), NAO com o php CLI -- o CLI nao emite
// headers HTTP nem preenche $_GET/$_POST a partir do ambiente CGI.
//
// GET  : le a query string        -> /cgi-php/info.php?nome=roberto
// POST : le o body do stdin (espera EOF, igual ao echo.py)

$metodo = getenv('REQUEST_METHOD') ?: '(sem REQUEST_METHOD)';
$query  = getenv('QUERY_STRING') ?: '';
$body   = ($metodo === 'POST') ? file_get_contents('php://input') : '';

// header() do php-cgi ja emite o CRLF e a linha em branco na hora certa
header('Content-Type: text/plain');

echo "PHP CGI ok\n";
echo "versao      : " . PHP_VERSION . "\n";
echo "metodo      : $metodo\n";
echo "query       : $query\n";

// as variaveis que o servidor precisa entregar, conforme a RFC 3875
foreach (array('SERVER_PROTOCOL', 'SCRIPT_NAME', 'PATH_INFO', 'CONTENT_LENGTH') as $k) {
    echo str_pad($k, 12) . ": " . (getenv($k) !== false ? getenv($k) : '-') . "\n";
}

// prova que o chdir do filho funciona: caminho relativo tem que abrir
echo "cwd         : " . basename(getcwd()) . "\n";
echo "vizinhos    : " . implode(', ', array_slice(array_diff(scandir('.'), array('.', '..')), 0, 3)) . "\n";

if ($metodo === 'POST') {
    echo "body len    : " . strlen($body) . "\n";
    echo "body        : $body\n";
}
