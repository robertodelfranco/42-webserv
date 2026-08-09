# CGI "comportado": headers com CRLF, como manda o RFC.
import sys

body = "<h1>Hello from CGI!</h1>\n"
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)
