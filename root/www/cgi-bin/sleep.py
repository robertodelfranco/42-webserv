# Chef que dorme no ponto: prova de concorrencia (M2) e de timeout/504 (M4).
import time
import sys

time.sleep(60)
sys.stdout.write("Content-Type: text/plain\r\n\r\nacordei!\n")
