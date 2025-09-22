

# set virtual env
# python3 -m venv stt-env
# source stt-env/bin/activate
#
# pip install "numpy<2"
# pip install -U openai-whisper
# brew install ffmpeg (macOS)
import socket
import whisper

HOST = '127.0.0.1'  # Localhost only
PORT = 5000         # Same port your C++ client connects to

g_model = whisper.load_model("medium")  # or "tiny", "small", "medium", "large"

# def mock_stt_process(audio_data: bytes) -> str:

#     # with tempfile.NamedTemporaryFile(suffix=".m4a", delete=True) as tmp:
#     #     tmp.write(audio_data)
#     #     tmp.flush()
#     #     result = g_model.transcribe(tmp.name)
#     #     print(f"[STT] Transcribed text: {result['text']}")
#     #     return result["text"]
#     # result = model.transcribe("example.wav")
#     result = g_model.transcribe("test1.m4a")
#     print(result["text"])
#     return result
#     # Replace with real speech-to-text logic
#     # return "You said: 'Hello, world!'"

def mock_stt_process(audio_data: bytes) -> str:
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=True) as tmp:
        tmp.write(audio_data)
        tmp.flush()
        result = g_model.transcribe(tmp.name)
        print(f"[STT] Transcribed text: {result['text']}")
        return result["text"]  # Return only the text string


import threading

def handle_client(client_socket):
    with client_socket:
        data = b''
        while True:
            chunk = client_socket.recv(4096)
            if not chunk:
                break
            data += chunk

        print(f"[STT] Received {len(data)} bytes")
        result = mock_stt_process(data)
        response = result + "\r\n"
        client_socket.sendall(response.encode('utf-8'))
        print(f"[STT] Sent response: {response}")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, PORT))
    server_socket.listen(1)
    print(f"[STT] Listening on {HOST}:{PORT}")

    while True:
        client_socket, addr = server_socket.accept()
        print(f"[STT] Connection from {addr}")
        threading.Thread(target=handle_client, args=(client_socket,), daemon=True).start()

#     while True:
#         client_socket, addr = server_socket.accept()
#         print(f"[STT] Connection from {addr}")
#         with client_socket:
#             data = client_socket.recv(4096)
#             if not data:
#                 continue

#             print(f"[STT] Received {len(data)} bytes")

#             # Process data (e.g., audio), get text result
#             result = mock_stt_process(data) + "\r\n"

#             # Send back result as UTF-8 string
#             client_socket.sendall(result.encode('utf-8'))
#             print(f"[STT] Sent response: {result}")
