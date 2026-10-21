"""Small SSH transport: verified host keys, no passwords in arguments or logs."""
import getpass
import json
import os
from pathlib import Path
import shlex
import sys

import paramiko


def connect(config, password_stdin=False):
    client = paramiko.SSHClient()
    client.load_system_host_keys()
    known = Path(config.get("knownHosts", "~/.ssh/known_hosts")).expanduser()
    if known.is_file():
        client.load_host_keys(str(known))
    client.set_missing_host_key_policy(paramiko.RejectPolicy())
    password = None
    if password_stdin:
        password = sys.stdin.readline().rstrip("\r\n")
        if not password:
            raise ValueError("Missing SSH password on stdin")
    elif config.get("authentication", "key") == "password":
        password = os.environ.get("ONEUI_SSH_PASSWORD") or getpass.getpass("SSH password: ")
    client.connect(config["host"], port=config.get("port", 22), username=config["user"],
                   password=password, key_filename=config.get("identityFile"),
                   allow_agent=password is None, look_for_keys=password is None,
                   timeout=20, auth_timeout=20, banner_timeout=20)
    return client


def run(client, args, payload=None, timeout=120):
    command = shlex.join(args)
    stdin, stdout, stderr = client.exec_command(command, timeout=timeout)
    if payload is not None:
        stdin.write(payload)
        stdin.flush()
    stdin.channel.shutdown_write()
    output = stdout.read().decode("utf-8", "replace")
    error = stderr.read().decode("utf-8", "replace")
    code = stdout.channel.recv_exit_status()
    if code:
        raise RuntimeError(f"Remote command failed ({code}): {error[-3000:]}")
    return output


def python(client, source, payload=None):
    return run(client, ["python3", "-c", source],
               None if payload is None else json.dumps(payload))
