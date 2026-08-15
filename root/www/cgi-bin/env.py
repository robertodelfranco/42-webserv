# Dump das meta-variaveis. DE PROPOSITO usa \n puro no fim dos headers
# (print padrao): seu CgiOutputParser precisa aceitar \n\n alem de \r\n\r\n.
import os

print("Content-Type: text/plain")
print()
for key in sorted(os.environ):
    print(key + "=" + os.environ[key])
