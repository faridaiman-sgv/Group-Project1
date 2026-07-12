import socket
import json
import time
import os

SERVER_HOST = os.environ.get('SERVER_HOST', 'python-server1')
SERVER_PORT = int(os.environ.get('SERVER_PORT', 5001))

while True:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_HOST, SERVER_PORT))
        sock.send(b"GET_POINTS")
        response = sock.recv(1024).decode()
        data = json.loads(response)
        print(f"[Python Client] User: {data['user']}, Points: {data['points']}, Time: {data['datetime_stamp']}")
        sock.close()
    except Exception as e:
        print(f"[Python Client] Error: {e}")
    time.sleep(10)
