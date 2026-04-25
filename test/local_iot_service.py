from __future__ import annotations

import argparse
import json
import queue
import socket
import threading
import time
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler
from http.server import ThreadingHTTPServer
from typing import Any


DEFAULT_HTTP_PORT = 8800
DEFAULT_MQTT_PORT = 1883
DEFAULT_DEVICE_SECRET = "LOCAL-CPR-SECRET"


@dataclass
class AuthRequestRecord:
    device_id: str
    module_id: str
    random_value: str
    sign: str
    raw_payload: dict[str, Any]
    received_at: float


@dataclass
class MqttMessageRecord:
    topic: str
    payload: bytes
    qos: int
    retain: bool
    received_at: float


class LocalIotService:
    def __init__(self, bind_host: str, http_port: int, mqtt_port: int, device_secret: str) -> None:
        self.bind_host = bind_host
        self.http_port = http_port
        self.mqtt_port = mqtt_port
        self.device_secret = device_secret
        self.auth_requests: list[AuthRequestRecord] = []
        self._auth_lock = threading.Lock()
        self._messages: "queue.Queue[MqttMessageRecord]" = queue.Queue()
        self._http_server: ThreadingHTTPServer | None = None
        self._http_thread: threading.Thread | None = None
        self._mqtt_socket: socket.socket | None = None
        self._broker_thread: threading.Thread | None = None
        self._broker_started = threading.Event()
        self._stop_event = threading.Event()
        self._subscriber_lock = threading.Lock()
        self._subscribers: list[tuple[socket.socket, str]] = []

    def start(self) -> None:
        self._start_broker()
        self._start_http_server()

    def stop(self) -> None:
        self._stop_event.set()
        if self._mqtt_socket is not None:
            try:
                self._mqtt_socket.close()
            except OSError:
                pass
        with self._subscriber_lock:
            subscribers = [sock for sock, _ in self._subscribers]
            self._subscribers.clear()
        for subscriber in subscribers:
            try:
                subscriber.close()
            except OSError:
                pass

        if self._http_server is not None:
            self._http_server.shutdown()
            self._http_server.server_close()
        if self._http_thread is not None:
            self._http_thread.join(timeout=5.0)

        if self._broker_thread is not None:
            self._broker_thread.join(timeout=10.0)

    def wait_for_auth(self, timeout: float) -> AuthRequestRecord:
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._auth_lock:
                if self.auth_requests:
                    return self.auth_requests[-1]
            time.sleep(0.1)
        raise TimeoutError("Timed out waiting for HTTP auth request")

    def wait_for_messages(self, expected_count: int, timeout: float) -> list[MqttMessageRecord]:
        records: list[MqttMessageRecord] = []
        deadline = time.time() + timeout
        while len(records) < expected_count:
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError(f"Timed out waiting for {expected_count} MQTT messages")
            try:
                records.append(self._messages.get(timeout=remaining))
            except queue.Empty as exc:
                raise TimeoutError(f"Timed out waiting for {expected_count} MQTT messages") from exc
        return records

    def wait_for_subscription(self, timeout: float, topic: str | None = None) -> str:
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._subscriber_lock:
                for _, subscribed_topic in self._subscribers:
                    if topic is None or topic == subscribed_topic:
                        return subscribed_topic
            time.sleep(0.1)
        expected = topic if topic is not None else "any topic"
        raise TimeoutError(f"Timed out waiting for MQTT subscription to {expected}")

    def publish_to_subscribers(self, topic: str, payload: bytes) -> int:
        body = len(topic).to_bytes(2, "big") + topic.encode("utf-8") + payload
        sent_count = 0
        stale: list[socket.socket] = []
        with self._subscriber_lock:
            subscribers = list(self._subscribers)
        for sock, subscribed_topic in subscribers:
            if subscribed_topic != topic:
                continue
            try:
                self._send_packet(sock, 0x30, body)
                sent_count += 1
            except OSError:
                stale.append(sock)
        if stale:
            with self._subscriber_lock:
                self._subscribers = [(sock, sub_topic) for sock, sub_topic in self._subscribers if sock not in stale]
        print(f"[mqtt] host publish topic={topic} len={len(payload)} subscribers={sent_count}")
        return sent_count

    def _record_auth_request(self, payload: dict[str, Any]) -> None:
        record = AuthRequestRecord(
            device_id=str(payload.get("deviceId", "")),
            module_id=str(payload.get("moduleId", "")),
            random_value=str(payload.get("random", "")),
            sign=str(payload.get("sign", "")),
            raw_payload=payload,
            received_at=time.time(),
        )
        with self._auth_lock:
            self.auth_requests.append(record)
        print(f"[http] auth deviceId={record.device_id} moduleId={record.module_id}")

    def _start_http_server(self) -> None:
        service = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "LocalIotHttp/1.0"

            def do_POST(self) -> None:
                if self.path != "/device/secret-key":
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                content_length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(content_length)
                try:
                    payload = json.loads(body.decode("utf-8"))
                except json.JSONDecodeError:
                    self.send_error(HTTPStatus.BAD_REQUEST, "invalid json")
                    return

                service._record_auth_request(payload)
                response = json.dumps({"code": 200, "result": service.device_secret}).encode("utf-8")
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)

            def log_message(self, format: str, *args: object) -> None:
                print(f"[http] {self.address_string()} {format % args}")

        self._http_server = ThreadingHTTPServer((self.bind_host, self.http_port), Handler)
        self._http_thread = threading.Thread(target=self._http_server.serve_forever, daemon=True)
        self._http_thread.start()
        print(f"[http] listening on {self.bind_host}:{self.http_port}")

    def _start_broker(self) -> None:
        def broker_thread() -> None:
            listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listen_socket.bind((self.bind_host, self.mqtt_port))
            listen_socket.listen(4)
            listen_socket.settimeout(0.5)
            self._mqtt_socket = listen_socket
            self._broker_started.set()
            while not self._stop_event.is_set():
                try:
                    client_socket, address = listen_socket.accept()
                except TimeoutError:
                    continue
                except OSError:
                    break
                thread = threading.Thread(target=self._handle_mqtt_client, args=(client_socket, address), daemon=True)
                thread.start()

        self._broker_thread = threading.Thread(target=broker_thread, daemon=True)
        self._broker_thread.start()
        if not self._broker_started.wait(timeout=10.0):
            raise TimeoutError("Timed out starting MQTT broker")
        print(f"[mqtt] broker listening on {self.bind_host}:{self.mqtt_port}")

    @staticmethod
    def _recv_exact(sock: socket.socket, size: int) -> bytes:
        buffer = bytearray()
        while len(buffer) < size:
            chunk = sock.recv(size - len(buffer))
            if not chunk:
                raise ConnectionError("socket closed")
            buffer.extend(chunk)
        return bytes(buffer)

    @staticmethod
    def _recv_remaining_length(sock: socket.socket) -> int:
        multiplier = 1
        value = 0
        while True:
            encoded = LocalIotService._recv_exact(sock, 1)[0]
            value += (encoded & 0x7F) * multiplier
            if (encoded & 0x80) == 0:
                return value
            multiplier *= 128

    @staticmethod
    def _encode_remaining_length(length: int) -> bytes:
        encoded = bytearray()
        while True:
            digit = length % 128
            length //= 128
            if length > 0:
                digit |= 0x80
            encoded.append(digit)
            if length == 0:
                return bytes(encoded)

    @staticmethod
    def _read_utf8_field(buffer: bytes, offset: int) -> tuple[str, int]:
        length = (buffer[offset] << 8) | buffer[offset + 1]
        offset += 2
        text = buffer[offset:offset + length].decode("utf-8", errors="replace")
        return text, offset + length

    def _send_packet(self, sock: socket.socket, fixed_header: int, body: bytes) -> None:
        packet = bytes([fixed_header]) + self._encode_remaining_length(len(body)) + body
        sock.sendall(packet)

    def _handle_mqtt_client(self, client_socket: socket.socket, address: tuple[str, int]) -> None:
        client_socket.settimeout(2.0)
        try:
            while not self._stop_event.is_set():
                try:
                    first_byte = self._recv_exact(client_socket, 1)[0]
                except TimeoutError:
                    continue
                except (ConnectionError, OSError):
                    return
                remaining_length = self._recv_remaining_length(client_socket)
                body = self._recv_exact(client_socket, remaining_length) if remaining_length > 0 else b""
                packet_type = first_byte >> 4
                if packet_type == 1:
                    self._handle_mqtt_connect(client_socket, address, body)
                elif packet_type == 3:
                    self._handle_mqtt_publish(client_socket, first_byte, body)
                elif packet_type == 8:
                    self._handle_mqtt_subscribe(client_socket, body)
                elif packet_type == 12:
                    self._send_packet(client_socket, 0xD0, b"")
                elif packet_type == 14:
                    return
                else:
                    print(f"[mqtt] ignore packet type={packet_type} from {address[0]}:{address[1]}")
        finally:
            with self._subscriber_lock:
                self._subscribers = [(sock, topic) for sock, topic in self._subscribers if sock is not client_socket]
            try:
                client_socket.close()
            except OSError:
                pass

    def _handle_mqtt_connect(self, client_socket: socket.socket, address: tuple[str, int], body: bytes) -> None:
        offset = 0
        protocol_name, offset = self._read_utf8_field(body, offset)
        protocol_level = body[offset]
        offset += 1
        connect_flags = body[offset]
        offset += 1
        keepalive = (body[offset] << 8) | body[offset + 1]
        offset += 2
        client_id, offset = self._read_utf8_field(body, offset)

        username = ""
        if connect_flags & 0x80:
            username, offset = self._read_utf8_field(body, offset)
        if connect_flags & 0x40:
            _, offset = self._read_utf8_field(body, offset)

        print(
            f"[mqtt] connect from {address[0]}:{address[1]} proto={protocol_name}/{protocol_level} "
            f"clientId={client_id} username={username} keepalive={keepalive}"
        )
        self._send_packet(client_socket, 0x20, b"\x00\x00")

    def _handle_mqtt_publish(self, client_socket: socket.socket, first_byte: int, body: bytes) -> None:
        offset = 0
        topic, offset = self._read_utf8_field(body, offset)
        qos = (first_byte >> 1) & 0x03
        retain = (first_byte & 0x01) != 0
        packet_id = None
        if qos > 0:
            packet_id = (body[offset] << 8) | body[offset + 1]
            offset += 2
        payload = body[offset:]
        record = MqttMessageRecord(
            topic=topic,
            payload=payload,
            qos=qos,
            retain=retain,
            received_at=time.time(),
        )
        self._messages.put(record)
        print(
            f"[mqtt] recv topic={record.topic} payload_len={len(record.payload)} "
            f"payload_hex={record.payload.hex()} payload={record.payload.decode('utf-8', errors='replace')}"
        )
        if qos == 1 and packet_id is not None:
            self._send_packet(client_socket, 0x40, bytes([(packet_id >> 8) & 0xFF, packet_id & 0xFF]))

    def _handle_mqtt_subscribe(self, client_socket: socket.socket, body: bytes) -> None:
        packet_id = body[:2]
        offset = 2
        granted = bytearray()
        while offset < len(body):
            topic, offset = self._read_utf8_field(body, offset)
            if offset >= len(body):
                break
            granted.append(min(body[offset], 1))
            offset += 1
            with self._subscriber_lock:
                if (client_socket, topic) not in self._subscribers:
                    self._subscribers.append((client_socket, topic))
            print(f"[mqtt] subscribe topic={topic}")
        self._send_packet(client_socket, 0x90, packet_id + bytes(granted))


def main() -> int:
    parser = argparse.ArgumentParser(description="Run local HTTP and MQTT mock services for the device IoT flow")
    parser.add_argument("--bind-host", default="0.0.0.0")
    parser.add_argument("--http-port", type=int, default=DEFAULT_HTTP_PORT)
    parser.add_argument("--mqtt-port", type=int, default=DEFAULT_MQTT_PORT)
    parser.add_argument("--device-secret", default=DEFAULT_DEVICE_SECRET)
    args = parser.parse_args()

    service = LocalIotService(
        bind_host=args.bind_host,
        http_port=args.http_port,
        mqtt_port=args.mqtt_port,
        device_secret=args.device_secret,
    )
    service.start()
    try:
        print("[svc] running, press Ctrl+C to stop")
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        print("[svc] stopping")
    finally:
        service.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())