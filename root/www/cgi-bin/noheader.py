# Saida sem bloco de headers (sem linha em branco): CGI invalido -> 502.
import sys

sys.stdout.write("<h1>sem header nenhum</h1>")
