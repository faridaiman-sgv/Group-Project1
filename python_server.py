#!/usr/bin/env python3
import socket
import json
import threading
import time
import os
import mysql.connector
from datetime import datetime

DB_CONFIG = {
    'host': os.environ.get('DB_HOST', 'mysql-db'),
    'user': os.environ.get('DB_USER', 'itt440_user'),
    'password': os.environ.get('DB_PASSWORD', 'itt440_pass'),
    'database': os.environ.get('DB_NAME', 'itt440_db')
}

SERVER_PORT = int(os.environ.get('SERVER_PORT', 5001))
USER_NAME = os.environ.get('USER_NAME', 'python_user')

def connect_db():
    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        print(f"[Python Server {USER_NAME}] Connected to database")
        return conn
    except Exception as e:
        print(f"[Python Server {USER_NAME}] DB connection error: {e}")
        return None

def update_points():
    while True:
        try:
            conn = connect_db()
            if conn:
                cursor = conn.cursor()
                cursor.execute("UPDATE user_points SET points = points + 1, datetime_stamp = CURRENT_TIMESTAMP WHERE user = %s", (USER_NAME,))
                conn.commit()
                cursor.close()
                conn.close()
                print(f"[Python Server {USER_NAME}] Updated points")
        except Exception as e:
            print(f"[Python Server {USER_NAME}] Update error: {e}")
        time.sleep(30)

def handle_client(client_socket):
    try:
        data = client_socket.recv(1024).decode()
        print(f"[Python Server {USER_NAME}] Received: {data}")
        
        if data == "GET_POINTS":
            conn = connect_db()
            if conn:
                cursor = conn.cursor(dictionary=True)
                cursor.execute("SELECT user, points, datetime_stamp FROM user_points WHERE user = %s", (USER_NAME,))
                result = cursor.fetchone()
                cursor.close()
                conn.close()
                
                if result:
                    if isinstance(result['datetime_stamp'], datetime):
                        result['datetime_stamp'] = result['datetime_stamp'].strftime('%Y-%m-%d %H:%M:%S')
                    client_socket.send(json.dumps(result).encode())
                    print(f"[Python Server {USER_NAME}] Sent response")
                else:
                    client_socket.send(json.dumps({"error": "User not found"}).encode())
        else:
            client_socket.send(b"Invalid command")
    except Exception as e:
        print(f"[Python Server {USER_NAME}] Error: {e}")
    finally:
        client_socket.close()

def main():
    thread = threading.Thread(target=update_points, daemon=True)
    thread.start()
    
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', SERVER_PORT))
    server.listen(5)
    print(f"[Python Server {USER_NAME}] Listening on port {SERVER_PORT}")
    
    while True:
        client, addr = server.accept()
        print(f"[Python Server {USER_NAME}] Client from {addr}")
        t = threading.Thread(target=handle_client, args=(client,))
        t.start()

if __name__ == "__main__":
    main()
