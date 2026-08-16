# O teste que mata maquina de estado sequencial: le stdin E escreve stdout,
# bytes binarios, com os dois pipes passando de 64KB ao mesmo tempo.
import sys

data = sys.stdin.buffer.read()
sys.stdout.buffer.write(b"Content-Type: application/octet-stream\r\n\r\n")
sys.stdout.buffer.write(data)
