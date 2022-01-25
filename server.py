import daemon
import time
import os
import signal
import atexit
import spacy

from typing import cast
from flask import Flask, request

Port = 9095
app = Flask(__name__)
nlp = None

@app.route("/", methods=["POST"])
def compute_suggestions():
    global nlp

    data = cast(dict[str, str], request.get_json())
    result = nlp(data["input"])

    return { "suggestions": [ token.text for token in result ] }

# https://refspecs.linuxfoundation.org/FHS_3.0/fhs/ch05s09.html
# Since /var/lock is not accessible by user, we use XDG_RUNTIME_DIR
def try_lock(filename: str, *, lock_dir = None) -> tuple[str, int | None]:
    if lock_dir is None:
        lock_dir = os.getenv("XDG_RUNTIME_DIR")
        if lock_dir is None:
            raise ValueError("XDG_RUNTIME_DIR must be set if lock_dir was not specified")

    path = os.path.join(lock_dir, filename + ".lock")

    # Lock file should contain 10 bytes, matching this regex /' '*\d+\n/
    # Where number is PID of file that is owner of given lock
    content = str(os.getpid())
    content = " " * (10 - len(content)) + content + '\n'

    # If file exists, then we can create lock, if not then report that file does not exists
    try:
        with open(path, "r") as lock:
            return path, int(lock.readline().lstrip())
    except FileNotFoundError:
        with open(path, "wb") as lock:
            lock.write(bytes(content, 'ascii'))
            return path, None

def load(name : str) -> spacy.language.Language:
    print(f"Loading Spacy with {name}")
    start = time.time()
    nlp = spacy.load(name)
    end = time.time()
    print(f"Loaded in {round(end - start, 2)}s")
    return nlp

def main():
    while True:
        lock, pid = try_lock("nlp-menu-worker")
        if pid is not None:
            print("Worker was already running. Replacing with new instance!")
            print(f"Killing {pid}. New worker PID: {os.getpid()}")
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                # Since process does not exists this lock is not associated with any process
                # and is considered a bug
                os.unlink(lock)
        else:
            break

    # Make sure, that when application terminates lock is removed
    atexit.register(lambda: os.unlink(lock))

    global nlp
    nlp = load("pl_core_news_sm")


    print(f"Serving at port http://localhost:{Port}/")
    with daemon.DaemonContext():
        pass

nlp = load("pl_core_news_sm")
app.run(port=Port)
