# Calculadora simples: GET usa QUERY_STRING, POST le o body do stdin.
# Espera a=<num>&b=<num>&op=add|sub|mul|div e devolve {"result": ...} em JSON.
import os
import sys
from urllib.parse import parse_qs


def respond(body):
	sys.stdout.write("Content-Type: application/json\r\n")
	sys.stdout.write("\r\n")
	sys.stdout.write(body)
	sys.exit(0)


def error(msg):
	respond('{"error": "%s"}' % msg.replace('"', '\\"'))


method = os.environ.get("REQUEST_METHOD", "GET")

if method == "POST":
	length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
	raw = sys.stdin.read(length) if length else sys.stdin.read()
else:
	raw = os.environ.get("QUERY_STRING", "")

params = parse_qs(raw)


def get(name):
	values = params.get(name)
	return values[0] if values else None


a_raw = get("a")
b_raw = get("b")
op = get("op")

if a_raw is None or b_raw is None or op is None:
	error("faltam parametros: a, b, op")

try:
	a = float(a_raw)
	b = float(b_raw)
except ValueError:
	error("a e b precisam ser numeros")

if op == "add":
	result = a + b
elif op == "sub":
	result = a - b
elif op == "mul":
	result = a * b
elif op == "div":
	if b == 0:
		error("divisao por zero")
	result = a / b
else:
	error("op invalido: use add, sub, mul ou div")

if result == int(result):
	result = int(result)

respond('{"result": %s}' % result)
