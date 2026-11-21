# SPDX-License-Identifier: MPL-2.0
"""Send the R20's live PCM to Cortex Video over the local network."""
import argparse
import asyncio
import ipaddress
import secrets
import socket
import struct

import sounddevice as sd


def local_address():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect(("192.0.2.1", 9))  # The kernel chooses the interface; no packet is sent.
        return probe.getsockname()[0]


def choose_device(index):
    devices = sd.query_devices()
    if index is None:
        candidates = [i for i, d in enumerate(devices)
                      if d["max_input_channels"] and "r20" in d["name"].lower()]
        if not candidates:
            raise RuntimeError("Zoom R20 não encontrado. Ative Audio Interface → Stereo Mix e conecte ao PC.")
        index = next((i for i in candidates
                      if "WASAPI" in sd.query_hostapis(devices[i]["hostapi"])["name"]), candidates[0])
    return index, sd.query_devices(index, "input")


async def serve(host, port, device, description):
    channels = min(2, description["max_input_channels"])
    rate = round(description["default_samplerate"])
    if rate not in (44100, 48000):
        raise RuntimeError(f"Configure a entrada do Windows para 44,1 ou 48 kHz. Taxa atual: {rate}.")
    block_frames = rate // 25
    code = secrets.token_hex(8)
    loop = asyncio.get_running_loop()
    connected = False

    async def session(reader, writer):
        nonlocal connected
        owner = False
        try:
            supplied = await asyncio.wait_for(reader.readline(), timeout=4)
            if not secrets.compare_digest(supplied, (code + "\n").encode("ascii")):
                writer.write(b"NOPE")
                await writer.drain()
                return
            if connected:
                writer.write(b"BUSY")
                await writer.drain()
                return
            owner = connected = True
            queue = asyncio.Queue(maxsize=5)

            def enqueue(block):
                if queue.full():
                    queue.get_nowait()
                queue.put_nowait(block)

            def capture(indata, frames, time_info, status):
                loop.call_soon_threadsafe(enqueue, bytes(indata))

            with sd.RawInputStream(device=device, channels=channels, samplerate=rate, dtype="int16",
                                   blocksize=block_frames, callback=capture):
                writer.write(struct.pack("!4sIB3x", b"CVR1", rate, channels))
                await writer.drain()
                print("iPhone conectado. Enviando áudio do R20.", flush=True)
                while True:
                    writer.write(await asyncio.wait_for(queue.get(), timeout=3))
                    await asyncio.wait_for(writer.drain(), timeout=2)
        except (OSError, ValueError, TimeoutError, sd.PortAudioError) as error:
            if owner:
                print(f"Áudio interrompido: {error}", flush=True)
        finally:
            if owner:
                connected = False
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass

    async with await asyncio.start_server(session, host, port, limit=128, family=socket.AF_INET) as server:
        print(f"Entrada: {description['name']} · {rate} Hz · {channels} canais", flush=True)
        print(f"Endereço no Cortex Video: {host}:{port}\nCódigo: {code}", flush=True)
        print("Aguardando o iPhone. Ctrl+C para encerrar.", flush=True)
        await server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="listar as entradas de áudio")
    parser.add_argument("--device", type=int, help="índice da entrada, se houver mais de um R20")
    parser.add_argument("--bind", help="IPv4 do Wi-Fi ou Ethernet do PC")
    parser.add_argument("--port", type=int, default=8771)
    args = parser.parse_args()
    if args.list:
        print(sd.query_devices())
        return
    try:
        host = args.bind or local_address()
        address = ipaddress.IPv4Address(host)
        if not any(address in ipaddress.IPv4Network(network) for network in
                   ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "127.0.0.0/8")):
            raise ValueError("Informe um endereço IPv4 privado da sua rede local em --bind.")
        if not 1 <= args.port <= 65535:
            raise ValueError("Porta inválida.")
        device, description = choose_device(args.device)
        asyncio.run(serve(host, args.port, device, description))
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError, RuntimeError, sd.PortAudioError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
