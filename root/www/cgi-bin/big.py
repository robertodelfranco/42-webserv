# ~5MB de saida: forca o dreno incremental do stdout (>64KB de pipe).
import sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
chunk = "x" * 1024
for _ in range(5120):
    sys.stdout.write(chunk)
