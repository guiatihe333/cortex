# SPDX-License-Identifier: MPL-2.0
"""CORTEX VIDEO: the PC mirror's picture and sound to the iPhone app over the local network (WebRTC).

This bridge only introduces the two (the "handshake"): the mirror page on this PC asks it for the session and leaves
its offer; the app connects with the address and the code shown, takes the offer and leaves its answer. The video and
the sound then go straight from Chrome to the app (WebRTC, on the local network), not through here.

  the page (http://127.0.0.1:8770)  <->  HTTP 127.0.0.1:8782  (this PC only)
  the app (the iPhone)              <->  TCP  <LAN IPv4>:8783 (a line protocol: the code, OFFER, ANSWER)
"""
import argparse
import asyncio
import ipaddress
import json
import secrets
import socket

PAGE_PORT, APP_PORT = 8782, 8783
MAX_SDP = 64 * 1024
PAGE_ORIGINS = {f"http://{host}:{port}" for host in ("127.0.0.1", "localhost") for port in (8770, 8774)}


def local_address():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect(("192.0.2.1", 9))  # the kernel picks the interface; nothing is sent
        return probe.getsockname()[0]


class Session:
    def __init__(self, address):
        self.address = address
        self.code = secrets.token_hex(8)
        self.offer = None
        self.answer = None
        self.offer_ready = asyncio.Event()
        self.joined = False

    def new_offer(self, sdp):
        self.offer, self.answer, self.joined = sdp, None, False
        self.offer_ready.set()


async def page_server(session, reader, writer):
    """The mirror page: GET /session, POST /offer, GET /answer (CORS: the page is on another port)."""
    status, body, origin = 404, b"", ""
    try:
        head = await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), timeout=5)
        lines = head.decode("latin-1").split("\r\n")
        method, path = lines[0].split(" ")[:2]
        headers = {k.strip().lower(): v.strip() for k, v in (l.split(":", 1) for l in lines[1:] if ":" in l)}
        origin = headers.get("origin", "")
        if origin not in PAGE_ORIGINS:
            raise PermissionError("Origem do espelho inválida.")
        length = int(headers.get("content-length", "0"))
        if not 0 <= length <= MAX_SDP:
            raise ValueError("too large")
        data = await asyncio.wait_for(reader.readexactly(length), timeout=5) if length else b""
        if method == "OPTIONS":
            status = 204
        elif method == "GET" and path == "/session":
            status, body = 200, json.dumps({"address": f"{session.address}:{APP_PORT}", "code": session.code}).encode()
        elif method == "POST" and path == "/offer":
            offer = json.loads(data)
            if offer.get("type") != "offer" or not isinstance(offer.get("sdp"), str):
                raise ValueError("not an offer")
            session.new_offer(data.decode())
            print("Oferta do espelho recebida. Aguardando o iPhone.", flush=True)
            status, body = 200, b"{}"
        elif method == "GET" and path == "/answer":
            status, body = (200, session.answer.encode()) if session.answer else (204, b"")
        elif method == "DELETE" and path == "/offer":
            session.offer = session.answer = None
            session.joined = False
            session.offer_ready.clear()
            status = 204
    except PermissionError:
        status, body = 403, b""
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, asyncio.TimeoutError, ValueError, TypeError, AttributeError, OSError):
        status, body = 400, b""
    try:
        allowed = f"Access-Control-Allow-Origin: {origin}\r\nVary: Origin\r\n" if origin in PAGE_ORIGINS else ""
        writer.write((f"HTTP/1.1 {status} X\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n"
                      + allowed + "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                      "Access-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Private-Network: true\r\n"
                      "Connection: close\r\n\r\n").encode() + body)
        await writer.drain()
    except OSError:
        pass
    writer.close()


async def app_server(session, reader, writer):
    """The app: its code line; then OFFER <json> once the page has one; its ANSWER <json>; OK."""
    taken = None
    try:
        code = await asyncio.wait_for(reader.readline(), timeout=6)
        if not secrets.compare_digest(code, (session.code + "\n").encode("ascii")):
            writer.write(b"NOPE\n")
            await writer.drain()
            return
        await asyncio.wait_for(session.offer_ready.wait(), timeout=90)
        if session.joined or not session.offer:
            writer.write(b"BUSY\n")
            await writer.drain()
            return
        taken = session.offer
        session.joined = True
        writer.write(b"OFFER " + taken.encode() + b"\n")
        await writer.drain()
        line = await asyncio.wait_for(reader.readuntil(b"\n"), timeout=30)
        if len(line) > MAX_SDP or not line.startswith(b"ANSWER "):
            raise ValueError("no answer")
        answer = json.loads(line[7:])
        if answer.get("type") != "answer" or not isinstance(answer.get("sdp"), str):
            raise ValueError("not an answer")
        if session.offer is not taken:
            writer.write(b"STALE\n")
            await writer.drain()
            return
        session.answer = line[7:].decode().strip()
        session.offer_ready.clear()
        writer.write(b"OK\n")
        await writer.drain()
        print("iPhone conectado. O vídeo e o som vão direto do Chrome para o app.", flush=True)
    except (asyncio.IncompleteReadError, asyncio.TimeoutError, asyncio.LimitOverrunError, ValueError,
            TypeError, AttributeError, OSError):
        pass
    finally:
        if taken is not None and session.offer is taken and session.answer is None:
            session.joined = False
        writer.close()


async def main_async(host):
    session = Session(host)
    page = await asyncio.start_server(lambda r, w: page_server(session, r, w), "127.0.0.1", PAGE_PORT)
    app = await asyncio.start_server(lambda r, w: app_server(session, r, w), host, APP_PORT,
                                     limit=MAX_SDP, family=socket.AF_INET)
    print(f"Endereço no app: {host}:{APP_PORT}\nCódigo: {session.code}", flush=True)
    print("No espelho do PC, toque em Transmitir ao iPhone. Ctrl+C para encerrar.", flush=True)
    async with page, app:
        await asyncio.gather(page.serve_forever(), app.serve_forever())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", help="IPv4 do Wi-Fi ou Ethernet do PC")
    args = parser.parse_args()
    host = args.bind or local_address()
    address = ipaddress.IPv4Address(host)
    if not any(address in ipaddress.IPv4Network(n) for n in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "127.0.0.0/8")):
        parser.exit(1, "Informe um endereço IPv4 privado da sua rede local em --bind.\n")
    try:
        asyncio.run(main_async(host))
    except KeyboardInterrupt:
        pass
    except OSError as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
