import json
import socket
import subprocess
import time
import unittest

class ShellTests(unittest.TestCase):
    def run_shell(self, commands):
        return subprocess.run(['./minishell'], input=commands, text=True,
                              capture_output=True, timeout=10)

    def test_command_and_pipe(self):
        result = self.run_shell('printf hello | wc -c\nexit\n')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout.strip(), '5')

    def test_cd(self):
        result = self.run_shell('cd /tmp\npwd\nexit\n')
        self.assertEqual(result.returncode, 0)
        self.assertIn('/tmp', result.stdout)

    def test_history(self):
        result = self.run_shell('echo ok\nhistory\nexit\n')
        self.assertIn('1  echo ok', result.stdout)

    def test_invalid_command(self):
        result = self.run_shell('this_command_does_not_exist_987\nexit\n')
        self.assertIn('this_command_does_not_exist_987', result.stderr)

    def test_background_jobs(self):
        result = self.run_shell('sleep 2 &\njobs\nexit\n')
        self.assertIn('[background]', result.stdout)
        self.assertIn('[running]', result.stdout)

    def test_redirection(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            path = d + '/output.txt'
            result = self.run_shell(f'echo hello > {path}\ncat < {path}\nexit\n')
            self.assertEqual(result.stdout.strip(), 'hello')
            with open(path) as f:
                self.assertEqual(f.read(), 'hello\n')

    def test_status_server(self):
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            port = s.getsockname()[1]
        proc = subprocess.Popen(['./status_server', str(port)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            for _ in range(50):
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=0.2) as s:
                        s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n')
                        data = b''
                        while True:
                            chunk = s.recv(4096)
                            if not chunk: break
                            data += chunk
                        self.assertEqual(json.loads(data.split(b'\r\n\r\n', 1)[1])['service'], 'minishell-status')
                        return
                except (ConnectionRefusedError, TimeoutError):
                    time.sleep(0.02)
            self.fail('server did not start')
        finally:
            proc.terminate()
            proc.communicate(timeout=3)

if __name__ == '__main__': unittest.main()
