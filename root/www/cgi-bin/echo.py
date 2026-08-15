# Le o body inteiro do stdin (espera EOF!) e devolve com o tamanho.
# Se o servidor nao fechar a ponta de escrita do stdin, este script trava aqui.
import sys

data = sys.stdin.read()
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("len=%d\n" % len(data))
sys.stdout.write(data)
