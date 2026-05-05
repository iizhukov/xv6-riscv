import os
import signal
import subprocess
import time
import unittest


BINARY = "./logserver"
FIFO   = "/tmp/test_logserver.fifo"
LOG    = "/tmp/test_logserver.log"


def cleanup():
    for path in (FIFO, LOG):
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass


def start(extra=None, alarm_interval=100):
    args = [BINARY, "-f", FIFO, "-l", LOG, "-n", str(alarm_interval)]

    if extra:
        args += extra

    proc = subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    deadline = time.time() + 3
    while time.time() < deadline:
        if os.path.exists(FIFO):
            time.sleep(0.05)
            break

        time.sleep(0.02)

    return proc


def send(msg: str):
    fd = os.open(FIFO, os.O_WRONLY)
    try:
        os.write(fd, msg.encode())
    finally:
        os.close(fd)


def collect(proc, timeout=3):
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    return out, err


class TestLogServer(unittest.TestCase):
    def setUp(self):
        cleanup()

    def tearDown(self):
        cleanup()

    def test_basic_message(self):
        """Сообщение, отправленное в FIFO, появляется в выводе."""
        proc = start()

        send("привет мир\n")
        time.sleep(0.1)
        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("привет мир", out)

    def test_multiple_messages(self):
        """Несколько последовательных сообщений — все попадают в лог."""
        proc = start()
        for i in range(5):
            send(f"сообщение {i}\n")
            time.sleep(0.05)

        proc.send_signal(signal.SIGTERM)
        out, _ = collect(proc)

        for i in range(5):
            self.assertIn(f"сообщение {i}", out)

    def test_message_without_trailing_newline(self):
        """Сообщение без \\n на конце — сервер добавляет его сам."""
        proc = start()

        send("без перевода строки")
        time.sleep(0.1)
        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("без перевода строки", out)

        idx = out.index("без перевода строки")
        after = out[idx + len("без перевода строки")]
        self.assertEqual(after, "\n")

    def test_statistics_on_exit(self):
        """Статистика выводится при завершении."""
        proc = start()

        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("статистика", out)

    def test_statistics_accuracy(self):
        """Счётчики сообщений и байт соответствуют отправленным данным."""
        proc = start()

        msgs = ["первое\n", "второе\n", "третье\n"]
        total = sum(len(m.encode()) for m in msgs)
        for m in msgs:
            send(m)
            time.sleep(0.05)

        proc.send_signal(signal.SIGTERM)
        out, _ = collect(proc)

        self.assertIn("сообщений=3", out)
        self.assertIn(f"байт={total}", out)

    def test_sigusr1_dumps_stats(self):
        """SIGUSR1 выводит статистику без завершения сервера."""
        proc = start()
        
        send("тест\n")
        time.sleep(0.1)
        proc.send_signal(signal.SIGUSR1)
        time.sleep(0.1)

        self.assertIsNone(proc.poll(), "сервер не должен завершиться по SIGUSR1")

        proc.send_signal(signal.SIGTERM)
        out, _ = collect(proc)
        self.assertGreaterEqual(out.count("статистика"), 2)

    def test_sigterm_exits_cleanly(self):
        """SIGTERM завершает сервер с кодом 0 и сообщением."""
        proc = start()
        
        proc.send_signal(signal.SIGTERM)
        
        out, _ = collect(proc)
        self.assertEqual(proc.returncode, 0)
        self.assertIn("завершение по SIGTERM", out)

    def test_sigint_exits_cleanly(self):
        """SIGINT завершает сервер с кодом 0 и сообщением."""
        proc = start()

        proc.send_signal(signal.SIGINT)

        out, _ = collect(proc)
        self.assertEqual(proc.returncode, 0)
        self.assertIn("завершение по SIGINT", out)

    def test_sigquit_is_ignored(self):
        """SIGQUIT игнорируется — сервер продолжает работу."""
        proc = start()

        proc.send_signal(signal.SIGQUIT)
        time.sleep(0.2)

        self.assertIsNone(proc.poll(), "сервер не должен завершиться по SIGQUIT")

        proc.send_signal(signal.SIGTERM)
        collect(proc)
        self.assertEqual(proc.returncode, 0)

    def test_sigint_drains_open_fifo(self):
        """При SIGINT сервер дочитывает уже открытый канал до конца."""
        proc = start()

        wfd = os.open(FIFO, os.O_WRONLY)
        time.sleep(0.1)
        os.write(wfd, "часть 1\n".encode())
        time.sleep(0.05)

        proc.send_signal(signal.SIGINT)
        time.sleep(0.05)

        os.write(wfd, "часть 2\n".encode())
        os.close(wfd)

        out, _ = collect(proc)
        self.assertIn("часть 1", out)
        self.assertIn("часть 2", out)


    def test_fifo_removed_on_exit(self):
        """После завершения сервер удаляет FIFO."""
        proc = start()

        proc.send_signal(signal.SIGTERM)
        collect(proc)
        
        self.assertFalse(os.path.exists(FIFO))

    def test_fifo_reuse_existing(self):
        """Если FIFO уже существует — сервер использует его, не падает."""
        os.mkfifo(FIFO, 0o600)
        proc = start()

        self.assertIsNone(proc.poll())

        send("тест\n")
        time.sleep(0.1)
        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("тест", out)

    def test_fifo_regular_file_is_error(self):
        """Если на месте FIFO обычный файл — сервер завершается с ошибкой."""
        with open(FIFO, "w") as f:
            f.write("")
        proc = start()

        _, err = collect(proc, timeout=2)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("не является FIFO", err)

    # ── пульс ────────────────────────────────────────────────────────

    def test_heartbeat_fires(self):
        """Пульс появляется в выводе с заданным интервалом."""
        proc = start(alarm_interval=1)

        time.sleep(2.5)
        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("пульс", out)
        self.assertGreaterEqual(out.count("пульс"), 2)

    def test_heartbeat_stats_in_output(self):
        """Счётчик будильников в пульсе растёт."""
        proc = start(alarm_interval=1)

        time.sleep(3.5)
        proc.send_signal(signal.SIGTERM)

        out, _ = collect(proc)
        self.assertIn("будильников=3", out)

    def test_daemon_writes_to_log_file(self):
        """В режиме демона вывод идёт в файл, а не на stdout."""
        proc = start(extra=["-d"])
        proc.wait(timeout=2)

        deadline = time.time() + 3
        while time.time() < deadline:
            if os.path.exists(FIFO):
                break
            time.sleep(0.05)

        send("сообщение демону\n")
        time.sleep(0.1)

        subprocess.run(["pkill", "-f", f"logserver.*{FIFO}"], check=False)
        time.sleep(0.3)

        with open(LOG) as f:
            log = f.read()

        self.assertIn("демон", log)
        self.assertIn("сообщение демону", log)

    def test_help_flag(self):
        """--help выводит справку и завершается с кодом 0."""
        result = subprocess.run(
            [BINARY, "--help"],
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 0)
        self.assertIn("Использование", result.stdout)
        self.assertIn("SIGTERM", result.stdout)

    def test_short_help_flag(self):
        """-h выводит справку и завершается с кодом 0."""
        result = subprocess.run(
            [BINARY, "-h"],
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 0)
        self.assertIn("Использование", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
