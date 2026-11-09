"""Regressoes do shell pela interface de linha de comando (Linux)."""

import os
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest

class ShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="minishell-tests-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.folder = Path(cls.directory.name)
        cls.binary = cls.folder / "minishell"
        root = Path(__file__).resolve().parent.parent
        subprocess.run(
            ["gcc", "-Wall", "-Wextra", "-Werror", "-g",
             str(root / "main.c"), str(root / "parser.c"),
             str(root / "executor.c"), str(root / "tests/syscall_faults.c"),
             "-Wl,--wrap=read,--wrap=write,--wrap=wait", "-o", str(cls.binary)],
            check=True,
        )
        cls.env = dict(os.environ, LC_ALL="C")
        cls.env.pop("MINISHELL_TEST_FAULT", None)
        (cls.folder / "mini-shell-teste.txt").write_text(
            "Arquivo de teste do mini shell.\n", encoding="utf-8",
        )
        cls.child = cls.folder / "slow-child"
        cls.child.write_text("#!/bin/sh\nsleep 0.1\nprintf 'filho terminou\\n'\n")
        cls.child.chmod(0o755)
        (cls.folder / "sem_permissao").write_text("sem permissao de execucao\n")
        (cls.folder / "sem_permissao").chmod(0o644)

    def run_shell(self, data=b"", fault=None, **kwargs):
        env = dict(self.env)
        env.pop("MINISHELL_TEST_FAULT", None)
        if fault:
            env["MINISHELL_TEST_FAULT"] = fault
        options = dict(stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, env=env, cwd=self.folder,
                       start_new_session=True)
        options.update(kwargs)
        with subprocess.Popen([str(self.binary)], **options) as process:
            try:
                output, errors = process.communicate(data, timeout=5)
            except subprocess.TimeoutExpired:
                #Encerra tambem os filhos se um pipeline ficar bloqueado.
                os.killpg(process.pid, signal.SIGKILL)
                process.communicate()
                raise
            return subprocess.CompletedProcess(
                process.args, process.returncode, output, errors,
            )

    def test_read_error_is_reported_and_exits_with_failure(self):
        # Ler um diretorio provoca um erro real de read(), distinto de EOF.
        descriptor = os.open(self.folder, os.O_RDONLY)
        try:
            result = self.run_shell(data=None, stdin=descriptor)
        finally:
            os.close(descriptor)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Erro ao ler a entrada", result.stderr)

    def test_eof_exits_successfully(self):
        result = self.run_shell()
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> ")
        self.assertEqual(result.stderr, b"")

    def test_exit_exits_successfully(self):
        result = self.run_shell(b"exit\n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> ")
        self.assertEqual(result.stderr, b"")

    def test_read_retries_after_repeated_interruptions(self):
        result = self.run_shell(b"echo recuperado\n", fault="read_eintr")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> recuperado\n> ")
        self.assertEqual(result.stderr, b"")

    def test_prompt_error_is_reported_and_exits_with_failure(self):
        # /dev/full produz ENOSPC em uma escrita real, mesmo de poucos bytes.
        with open("/dev/full", "wb") as output:
            result = self.run_shell(b"exit\n", stdout=output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Erro ao exibir o prompt", result.stderr)

    def test_prompt_retries_after_repeated_interruptions(self):
        result = self.run_shell(b"exit\n", fault="write_eintr")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> ")
        self.assertEqual(result.stderr, b"")

    def test_prompt_completes_partial_writes(self):
        result = self.run_shell(b"exit\n", fault="write_partial")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> ")
        self.assertEqual(result.stderr, b"")

    def test_prompt_zero_write_does_not_loop_forever(self):
        result = self.run_shell(b"exit\n", fault="write_zero")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Erro ao exibir o prompt", result.stderr)

    def test_wait_error_is_reported(self):
        result = self.run_shell(b"/bin/true\n", fault="wait_error")
        self.assertIn(b"Erro ao aguardar o processo filho", result.stderr)

    def test_wait_retries_without_showing_prompt_before_child_finishes(self):
        result = self.run_shell(
            (str(self.child) + "\n").encode(), fault="wait_eintr",
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> filho terminou\n> ")
        self.assertEqual(result.stderr, b"")

    def test_required_commands_accept_arguments(self):
        cases = [
            (b"ls -1 mini-shell-teste.txt\n", b"mini-shell-teste.txt\n"),
            (b"cat mini-shell-teste.txt\n", b"Arquivo de teste do mini shell.\n"),
            (b"echo Ola mundo\n", b"Ola mundo\n"),
        ]
        for command, output in cases:
            with self.subTest(command=command):
                result = self.run_shell(command)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"> " + output + b"> ")
                self.assertEqual(result.stderr, b"")

    def test_unknown_command_reports_error_and_returns_to_prompt(self):
        result = self.run_shell(b"comando_inexistente_minishell\n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> > ")
        self.assertIn("Arquivo ou diretório não existente".encode(), result.stderr)

    def test_permission_error_returns_to_prompt(self):
        result = self.run_shell(b"./sem_permissao\n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> > ")
        self.assertIn(b"Acesso negado: ./sem_permissao", result.stderr)

    def test_empty_line_returns_to_prompt(self):
        result = self.run_shell(b"\n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")

if __name__ == "__main__":
    unittest.main(verbosity=2)
