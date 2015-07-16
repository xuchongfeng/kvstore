######################################################################
# THIS FILE IS A USEFUL HARNESS TO TEST AND UNDERSTAND THE BEHAVIORS #
# OF THE PROVIDED CLIENTS. FEEL FREE TO EDIT IT AS YOU WISH. IT WILL #
#              BE CONSIDERED AS PART OF YOUR SUBMISSION              #
######################################################################

from client import *
import socket, time

# You may or may not want to change these
host, port = "127.0.0.1", 8000

if __name__ == "__main__":
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((host, port))
    sock.listen(1)
    conn, addr = sock.accept()
    print 'Connection received from', addr
    try:
        data = conn.recv(1024)
        if not data: raise Exception
        print 'Data received:'
        print '\t', data
        msg = KVMessage(msg_type=RESPONSE, msg=SUCCESS)
        msg.send(conn)
    except Exception:
        pass
    conn.close()
    sock.close()
