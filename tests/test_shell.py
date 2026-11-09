"""Regressoes do shell pela interface de linha de comando (Linux)."""

import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time
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
             "-Wl,--wrap=read,--wrap=write,--wrap=wait,--wrap=pipe,"
             "--wrap=dup2,--wrap=fork,--wrap=open", "-o", str(cls.binary)],
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

    def test_output_redirection_creates_and_truncates_file(self):
        output = self.folder / "saida.txt"
        output.write_text("conteudo antigo que deve desaparecer\n")
        result = self.run_shell(b"echo novo > saida.txt\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")
        self.assertEqual(output.read_text(), "novo\n")

    def test_append_redirection_preserves_previous_contents(self):
        output = self.folder / "acrescentar.txt"
        output.write_text("primeira\n")
        result = self.run_shell(b"echo segunda >> acrescentar.txt\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")
        self.assertEqual(output.read_text(), "primeira\nsegunda\n")

    def test_input_redirection(self):
        result = self.run_shell(b"cat < mini-shell-teste.txt\n")
        self.assertEqual(result.stdout, b"> Arquivo de teste do mini shell.\n> ")
        self.assertEqual(result.stderr, b"")

    def test_pipeline_connects_two_commands(self):
        result = self.run_shell(b"echo ola mundo | tr a-z A-Z\n")
        self.assertEqual(result.stdout, b"> OLA MUNDO\n> ")
        self.assertEqual(result.stderr, b"")

    def test_pipeline_combines_redirections_without_spaces(self):
        result = self.run_shell(
            b"cat<mini-shell-teste.txt|tr a-z A-Z|cat>pipeline.txt\n",
        )
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")
        self.assertEqual(
            (self.folder / "pipeline.txt").read_text(),
            "ARQUIVO DE TESTE DO MINI SHELL.\n",
        )

    def test_pipeline_does_not_deadlock_with_more_than_pipe_capacity(self):
        result = self.run_shell(b"seq 1 50000 | wc -l\n")
        self.assertEqual(result.stdout, b"> 50000\n> ")
        self.assertEqual(result.stderr, b"")

    def test_pipeline_closes_unused_ends_when_consumer_exits_early(self):
        result = self.run_shell(b"yes | head -n 1\n")
        self.assertEqual(result.stdout, b"> y\n> ")
        self.assertEqual(result.stderr, b"")

    def test_file_redirection_overrides_pipe_output(self):
        result = self.run_shell(b"echo arquivo > prioridade.txt | cat\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")
        self.assertEqual((self.folder / "prioridade.txt").read_text(), "arquivo\n")

    def test_repeated_redirections_are_applied_in_order(self):
        first = self.folder / "primeiro.txt"
        second = self.folder / "segundo.txt"
        result = self.run_shell(b"echo destino > primeiro.txt > segundo.txt\n")
        self.assertEqual(result.stderr, b"")
        self.assertEqual(first.read_text(), "")
        self.assertEqual(second.read_text(), "destino\n")

    def test_missing_input_file_reports_error(self):
        result = self.run_shell(b"cat < arquivo_inexistente.txt\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertIn(b"Erro ao abrir", result.stderr)
        self.assertIn(b"arquivo_inexistente.txt", result.stderr)

    def test_invalid_syntax_does_not_execute_any_command(self):
        cases = [b"| echo proibido\n", b"echo proibido |\n",
                 b"echo proibido || cat\n", b"echo proibido >\n",
                 b"echo proibido > > arquivo\n", b"< mini-shell-teste.txt\n",
                 b"echo proibido >nao_criar.txt |\n", b"echo 'sem fechar\n",
                 b"exit >nao_criar.txt\n"]
        for command in cases:
            with self.subTest(command=command):
                result = self.run_shell(command)
                self.assertEqual(result.stdout, b"> > ")
                self.assertIn(b"Erro de sintaxe", result.stderr)
        self.assertFalse((self.folder / "nao_criar.txt").exists())

    def test_quotes_preserve_spaces_and_literal_operators(self):
        result = self.run_shell(b'echo "a|b>c" > "nome com espacos.txt"\n')
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")
        self.assertEqual((self.folder / "nome com espacos.txt").read_text(), "a|b>c\n")

    def test_single_quotes_tabs_and_empty_arguments(self):
        result = self.run_shell(b"printf\t'<%s>'\t''|cat\n")
        self.assertEqual(result.stdout, b"> <>> ")
        self.assertEqual(result.stderr, b"")

    def test_parser_rejects_limits_without_truncating_or_executing(self):
        commands = [
            "echo " + "a " * 63,
            "echo oi" + " | cat" * 16,
            "echo oi" + " > nao_criar_limite.txt" * 17,
        ]
        for command in commands:
            with self.subTest(command=command):
                result = self.run_shell((command + "\n").encode())
                self.assertEqual(result.stdout, b"> > ")
                self.assertIn(b"Erro de sintaxe: limite", result.stderr)
        self.assertFalse((self.folder / "nao_criar_limite.txt").exists())

    def test_pipe_creation_failure_does_not_start_commands(self):
        for fault in ("pipe_error", "pipe_after_one"):
            with self.subTest(fault=fault):
                result = self.run_shell(b"echo proibido | cat | cat\n", fault=fault)
                self.assertEqual(result.stdout, b"> > ")
                self.assertIn(b"Erro ao criar pipe", result.stderr)

    def test_fork_failure_cleans_up_an_incomplete_pipeline(self):
        for fault in ("fork_error", "fork_after_one"):
            with self.subTest(fault=fault):
                result = self.run_shell(b"sleep 30 | cat\n", fault=fault)
                self.assertEqual(result.stdout, b"> > ")
                self.assertIn(b"Erro ao criar processo filho", result.stderr)

    def test_dup2_error_does_not_execute_the_command(self):
        for command in (b"echo proibido > dup2.txt\n", b"echo proibido | cat\n"):
            with self.subTest(command=command):
                result = self.run_shell(command, fault="dup2_error")
                self.assertEqual(result.stdout, b"> > ")
                self.assertIn(b"Erro ao redirecionar descritor", result.stderr)
        self.assertEqual((self.folder / "dup2.txt").read_text(), "")

    def test_dup2_retries_after_repeated_interruptions(self):
        result = self.run_shell(b"echo recuperado | cat\n", fault="dup2_eintr")
        self.assertEqual(result.stdout, b"> recuperado\n> ")
        self.assertEqual(result.stderr, b"")

    def test_open_retries_after_repeated_interruptions(self):
        result = self.run_shell(b"cat < mini-shell-teste.txt\n", fault="open_eintr")
        self.assertEqual(result.stdout, b"> Arquivo de teste do mini shell.\n> ")
        self.assertEqual(result.stderr, b"")

    def test_output_open_failure_is_reported(self):
        result = self.run_shell(b"echo proibido > .\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertIn(b"Erro ao abrir", result.stderr)

    def test_exit_in_pipeline_does_not_exit_parent(self):
        result = self.run_shell(b"exit | cat\n")
        self.assertEqual(result.stdout, b"> > ")
        self.assertEqual(result.stderr, b"")

    def test_parent_keeps_its_io_and_does_not_leak_descriptors(self):
        with subprocess.Popen(
            [str(self.binary)], cwd=self.folder, env=self.env,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, start_new_session=True,
        ) as process:
            def receive_prompt():
                data = b""
                deadline = time.monotonic() + 5
                while not data.endswith(b"> "):
                    remaining = deadline - time.monotonic()
                    if remaining <= 0 or not select.select([process.stdout], [], [], remaining)[0]:
                        self.fail("Prompt nao recebido: " + repr(data))
                    chunk = os.read(process.stdout.fileno(), 4096)
                    self.assertTrue(chunk, "Shell encerrou antes do prompt")
                    data += chunk
                return data[:-2]

            try:
                self.assertEqual(receive_prompt(), b"")
                descriptors = Path(f"/proc/{process.pid}/fd")
                initial_count = len(list(descriptors.iterdir()))
                for _ in range(20):
                    for command, output in [
                        (b"echo preservado > interativo.txt\n", b""),
                        (b"cat < interativo.txt | tr a-z A-Z\n", b"PRESERVADO\n"),
                    ]:
                        process.stdin.write(command)
                        process.stdin.flush()
                        self.assertEqual(receive_prompt(), output)
                    self.assertEqual(len(list(descriptors.iterdir())), initial_count)
                    children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
                    self.assertEqual(children.read_text().strip(), "")
                output, errors = process.communicate(b"exit\n", timeout=5)
                self.assertEqual(process.returncode, 0)
                self.assertEqual(output, b"")
                self.assertEqual(errors, b"")
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.communicate()


if __name__ == "__main__":
    unittest.main(verbosity=2)
