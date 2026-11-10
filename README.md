# Mini Shell com Chamadas ao Sistema

Projeto de Sistemas Operacionais desenvolvido em C para o Projeto Integrador do Instituto de Computação da Universidade Federal da Bahia (UFBA), sob orientação da Profa. Dra. Larissa Barbosa Leôncio Pinheiro.

O objetivo é implementar um interpretador de comandos simplificado e aplicar conceitos de criação de processos, execução de programas, entrada e saída e sincronização entre processo pai e processo filho. O mini shell executa programas reais do sistema, como `ls`, `cat` e `echo`.

## 1. Funcionalidades

- Exibição do prompt interativo `> `.
- Leitura da entrada padrão com `read()`.
- Separação dos comandos e argumentos por espaços ou tabulações, com suporte básico a aspas simples e duplas.
- Criação de um processo filho por comando com `fork()`.
- Execução do programa solicitado com `execvp()`.
- Redirecionamento de entrada (`<`), saída com sobrescrita (`>`) e saída com acréscimo (`>>`), usando `open()` e `dup2()`.
- Conexão de comandos por pipes (`|`), usando `pipe()` e `dup2()`, inclusive sem espaços ao redor dos operadores.
- Espera pelo término de todos os filhos com `wait()` antes de aceitar outra linha de comandos.
- Mensagens de erro para falhas na leitura, na escrita do prompt, na criação e espera de processos e na execução de programas.
- Repetição de `read()`, `write()` e `wait()` quando interrompidos por sinais (`EINTR`).
- Encerramento pelo comando interno `exit` ou pelo fim da entrada (EOF).

## 2. Requisitos e ambiente

Para compilar e executar, é necessário um ambiente Linux com GCC, GNU Make e os cabeçalhos e bibliotecas de desenvolvimento C com suporte às interfaces POSIX utilizadas.

No Windows, use uma distribuição Linux no WSL. Os comandos das próximas seções devem ser executados no terminal Linux, dentro da pasta do projeto. A implementação utiliza interfaces como `fork()` e não é destinada à execução nativa no PowerShell.

Em Ubuntu ou Debian, caso as ferramentas de compilação ainda não estejam instaladas:

```bash
sudo apt update
sudo apt install build-essential
```

## 3. Como compilar e executar

Na pasta do projeto, compile com:

```bash
make
```

O `Makefile` compila `main.c`, `parser.c` e `executor.c`, gera os arquivos objeto correspondentes e os vincula ao executável `minishell`. São utilizadas as opções `-Wall` e `-Wextra` para habilitar avisos e `-g` para incluir informações de depuração.

Também é possível compilar diretamente:

```bash
gcc -Wall -Wextra -g main.c parser.c executor.c -o minishell
```

Para executar:

```bash
./minishell
```

O programa exibirá o prompt `> `. Digite um comando e pressione Enter. Por exemplo:

```text
> echo Ola mundo
Ola mundo
> exit
```

Para remover os arquivos objeto e o executável:

```bash
make clean
```

Para recompilar todos os arquivos:

```bash
make clean
make
```

## 4. Estrutura do projeto

| Arquivo | Responsabilidade |
| --- | --- |
| `main.c` | Controla o laço principal e conecta a leitura, a interpretação e a execução dos comandos. |
| `parser.c` | Exibe o prompt, lê a entrada e interpreta argumentos, aspas, pipes, redirecionamentos e o comando `exit`. |
| `parser.h` | Declara as funções do parser, os limites e as estruturas `Command`, `Redirection` e `Pipeline`. |
| `executor.c` | Cria os pipes e os filhos, conecta descritores, aplica redirecionamentos, executa os programas e aguarda todos os filhos. |
| `executor.h` | Declara a função `execute_pipeline()`. |
| `Makefile` | Define as regras de compilação, vinculação e limpeza. |
| `minishell` | Executável incluído no repositório; pode ser reconstruído a partir do código-fonte. |
| `LICENSE` | Contém o texto da licença GNU GPL versão 3. |
| `tests/` | Testes do shell e simulação controlada de falhas nas chamadas, usados apenas durante os testes. |

### Funções principais

| Função | Comportamento |
| --- | --- |
| `main()` | Repete o ciclo do shell enquanto houver entrada e o comando `exit` não for identificado. |
| `display_prompt()` | Escreve todos os bytes de `> `, completando escritas parciais. Retorna `0` no sucesso ou `-1` após informar uma falha. |
| `read_input(buffer)` | Lê até `MAX_INPUT - 1` bytes e acrescenta o terminador `\0`. Retorna a quantidade de bytes lidos ou um valor negativo em caso de erro. |
| `parse_input(buffer, pipeline)` | Monta os comandos e redirecionamentos. Retorna `0` para `exit` isolado, `1` para continuar ou `-1` para erro de sintaxe. Os vetores de argumentos terminam em `NULL`. |
| `execute_pipeline(pipeline)` | Cria e conecta os filhos do pipeline, aplica redirecionamentos e coleta seus términos. Também atende comandos simples, sem pipes. |

Cada `Command` guarda seus argumentos e uma lista ordenada de redirecionamentos. O `Pipeline` guarda os comandos e uma área de armazenamento para as palavras, copiadas da entrada sem os operadores e as aspas delimitadoras. Os argumentos e nomes de arquivos apontam para essa área. Não há alocação dinâmica explícita de memória no código do projeto.

## 5. Fluxo de funcionamento

1. `main()` chama `display_prompt()` para indicar que o shell está pronto. Se a escrita falhar, o shell encerra com status de falha.
2. `read_input()` recebe a entrada pelo descritor `STDIN_FILENO`.
3. Se a leitura retornar zero (EOF), o laço é encerrado normalmente. Se retornar um valor negativo após as tentativas por `EINTR`, o erro é informado e o programa encerra com status de falha.
4. `parse_input()` monta o pipeline e valida sua sintaxe antes de executar qualquer comando ou abrir arquivos. Uma linha vazia não gera processos; `exit` isolado encerra o laço.
5. O executor cria os pipes necessários e chama `fork()` uma vez para cada comando.
6. Cada filho conecta os pipes aos descritores padrão com `dup2()`, fecha as pontas originais, aplica os redirecionamentos e chama `execvp()`.
7. O pai fecha todas as suas pontas dos pipes e chama `wait()` até coletar todos os filhos. Depois, volta ao primeiro passo.

Por exemplo, a entrada `ls -l` gera o vetor:

```c
args[0] = "ls";
args[1] = "-l";
args[2] = NULL;
```

O ponteiro `NULL` indica o fim do vetor para `execvp()`. Se o nome do programa não contém `/`, essa função procura o executável nos diretórios de `PATH`. Também é possível informar um caminho, como `/bin/ls`.

O comando `exit`, quando aparece sozinho na linha, é tratado pelo próprio shell, antes de `fork()`. Se fizer parte de um pipeline, encerra apenas o filho correspondente. Encerrar apenas um filho não encerra o processo pai que mantém o laço principal.

### Pipes e redirecionamentos

Em `echo ola | tr a-z A-Z`, o primeiro filho usa a ponta de escrita do pipe como saída padrão, e o segundo usa a ponta de leitura como entrada padrão. Todos os filhos são iniciados antes de o pai esperar por qualquer um deles. Isso permite que produtor e consumidor trabalhem simultaneamente, mesmo quando os dados ultrapassam a capacidade do pipe.

As pontas originais são fechadas nos filhos e no pai. Manter uma ponta de escrita aberta sem uso impediria o consumidor de receber EOF; manter uma ponta de leitura aberta sem uso poderia impedir o produtor de perceber que o consumidor terminou.

Os redirecionamentos são aplicados apenas nos filhos, sem alterar a entrada ou a saída do próprio shell:

| Operador | Abertura do arquivo | Efeito de `dup2()` |
| --- | --- | --- |
| `<` | `O_RDONLY` | Substitui a entrada padrão (descritor 0). |
| `>` | `O_WRONLY` com `O_CREAT` e `O_TRUNC` | Substitui a saída padrão (descritor 1), criando ou esvaziando o arquivo. |
| `>>` | `O_WRONLY` com `O_CREAT` e `O_APPEND` | Substitui a saída padrão (descritor 1), preservando o conteúdo anterior e escrevendo ao final. |

Arquivos novos são abertos com modo `0666`, sujeito à `umask` do ambiente. Os redirecionamentos são aplicados na ordem escrita e depois das conexões dos pipes. Por exemplo, `echo oi > a.txt > b.txt` cria ou esvazia os dois arquivos e escreve em `b.txt`; `echo oi > a.txt | cat` escreve no arquivo, enquanto `cat` recebe EOF pelo pipe.

## 6. Chamadas e interfaces utilizadas

As funções abaixo permitem acessar os serviços de entrada e saída e de gerenciamento de processos do sistema operacional por meio da biblioteca C. `execvp()` é uma função da família `exec` que oferece a busca em `PATH`; no Linux, a substituição do programa é realizada por uma chamada ao sistema subjacente, como `execve()`.

| Interface | Local de uso | Papel no projeto |
| --- | --- | --- |
| `write()` | `parser.c`, em `display_prompt()` | Escreve o prompt no descritor `STDOUT_FILENO` (1). |
| `read()` | `parser.c`, em `read_input()` | Lê bytes do descritor `STDIN_FILENO` (0). |
| `fork()` | `executor.c`, em `execute_pipeline()` | Cria um processo filho por comando a partir do processo do shell. |
| `execvp()` | Ramo do filho em `executor.c` | Substitui o programa do filho pelo comando solicitado, mantendo o PID do filho. |
| `wait()` | Ramo do pai em `executor.c` | Aguarda um filho e coleta suas informações de término. |
| `_exit(127)` | Após falha de `execvp()` em `executor.c` | Encerra o filho com código 127 quando não foi possível executar o programa. |
| `pipe()` | `executor.c` | Cria um canal com uma ponta de leitura e outra de escrita entre comandos vizinhos. |
| `dup2()` | `executor.c` | Conecta arquivos e pipes à entrada ou à saída padrão do filho. |
| `open()` | `executor.c` | Abre ou cria os arquivos dos redirecionamentos. |
| `close()` | `executor.c` | Fecha os descritores originais dos arquivos e as pontas dos pipes que não serão mais usadas. |
| `kill()` | Limpeza de falha de `fork()` em `executor.c` | Encerra os filhos já iniciados de um pipeline incompleto antes de coletá-los. |

### Criação, execução e espera

O retorno de `fork()` permite distinguir três situações:

- Valor negativo: houve falha e nenhum filho foi criado; o shell mostra uma mensagem com `perror()`.
- Zero: o código está executando no filho, que chama `execvp()`.
- Valor positivo: o código está executando no pai; o valor retornado é o PID do filho.

Quando `execvp()` tem sucesso, ela não retorna ao código do mini shell: o filho passa a executar o programa solicitado. Se falhar, retorna ao executor, que mostra o erro e chama `_exit(127)`. Isso impede que o filho continue executando o laço do shell após uma falha.

O pai aguarda em `wait()`, sem realizar espera ocupada, depois de iniciar todos os filhos. Os comandos de um pipeline executam concorrentemente em primeiro plano; uma nova linha de comandos só é aceita após a coleta de todos os filhos. A coleta evita que permaneçam como processos zumbis após as esperas bem-sucedidas. O código recebe o estado na variável `status`, mas não o interpreta nem o apresenta ao usuário.

### Tratamento de erros

Quando `execvp()` falha, o executor consulta `errno`:

| Condição | Mensagem do projeto |
| --- | --- |
| `EPERM` | `Permissão negada ao executar: <comando>` |
| `ENOENT` | `Arquivo ou diretório não existente: <comando>` |
| `EACCES` | `Acesso negado: <comando>` |
| Outros erros | Mensagem produzida por `perror()`, com o nome do comando. |

As mensagens são enviadas à saída de erros. Após uma falha de execução no filho, o processo pai volta a apresentar o prompt.

As demais operações também têm seus retornos verificados:

| Operação | Tratamento |
| --- | --- |
| `read()` | Repete em caso de `EINTR`. Para outros erros, informa `Erro ao ler a entrada` e o shell encerra com `EXIT_FAILURE`. EOF continua sendo encerramento normal. |
| `write()` do prompt | Repete em caso de `EINTR` e completa escritas parciais. Em caso de erro ou ausência de progresso, informa `Erro ao exibir o prompt` e o shell encerra com `EXIT_FAILURE`. |
| `wait()` | Repete em caso de `EINTR`, mantendo a espera pelo filho. Outros erros são informados com `Erro ao aguardar o processo filho`, e o shell volta ao laço principal. |
| `open()` | Repete em caso de `EINTR`. Outros erros informam o nome do arquivo e encerram apenas o filho afetado, antes de executar o programa. |
| `dup2()` | Repete em caso de `EINTR`. Outros erros informam `Erro ao redirecionar descritor` e encerram apenas o filho afetado. |
| `pipe()` | Informa `Erro ao criar pipe`, fecha os pipes já criados e não inicia os comandos da linha. |
| `fork()` | Informa `Erro ao criar processo filho`. Se a falha ocorrer após iniciar parte do pipeline, fecha os pipes, cancela os filhos já iniciados e coleta seus términos. |
| `close()` e `kill()` | Informam falhas na limpeza de descritores e de processos. A ausência de um processo ao tentar cancelá-lo (`ESRCH`) é tolerada. |

Erros de sintaxe, como pipe sem comando, redirecionamento sem arquivo ou aspas não fechadas, também recebem uma mensagem e devolvem o controle ao prompt. Nenhum comando da linha é iniciado nesses casos.

## 7. Comandos testados e saídas

Os exemplos abaixo foram verificados no Ubuntu 24.04 via WSL. A suíte automatizada também recompila o código-fonte com `-Wall -Wextra -Werror -g` e verifica comandos básicos, pipelines, redirecionamentos e tratamentos de erro. Cada entrada contém uma linha de comandos, evitando agrupar várias linhas em uma única leitura.

Para reproduzir os exemplos, prepare o arquivo de teste **no terminal Linux, antes de iniciar o mini shell**:

```bash
printf 'Arquivo de teste do mini shell.\n' > mini-shell-teste.txt
./minishell
```

### Listagem com argumento: `ls`

```text
> ls -1 mini-shell-teste.txt
mini-shell-teste.txt
```

A opção `-1` solicita um nome por linha. O argumento seguinte indica o arquivo a listar.

### Leitura de arquivo com argumento: `cat`

```text
> cat mini-shell-teste.txt
Arquivo de teste do mini shell.
```

### Escrita de mensagem com argumentos: `echo`

```text
> echo Ola mundo
Ola mundo
```

`Ola` e `mundo` são argumentos separados. O programa `echo` os imprime separados por espaço.

### Comando inexistente

```text
> comando_inexistente_minishell
Arquivo ou diretório não existente: comando_inexistente_minishell
```

O shell permanece ativo após exibir a mensagem.

### Arquivo sem permissão de execução

Para preparar este caso, execute no terminal Linux, antes de iniciar o mini shell:

```bash
printf 'arquivo sem permissao de execucao\n' > sem_permissao
chmod 644 sem_permissao
```

No mini shell, foi observada a saída:

```text
> ./sem_permissao
Acesso negado: ./sem_permissao
```

### Outros comportamentos verificados

| Teste | Resultado observado |
| --- | --- |
| Linha vazia | Um novo prompt é exibido, sem mensagem de erro. |
| `sleep 1` | O próximo prompt aparece após aproximadamente um segundo, confirmando a espera pelo comando. |
| `exit` | O mini shell encerra com código de saída 0. |
| Entrada encerrada (EOF) | O mini shell encerra com código de saída 0. Em um terminal no modo usual, `Ctrl+D` em uma linha vazia permite produzir essa condição. |

### Redirecionamentos e pipes

Digite uma linha de cada vez no mini shell:

```text
> echo primeira > saida.txt
> echo segunda >> saida.txt
> cat < saida.txt
primeira
segunda
> cat < saida.txt | tr a-z A-Z
PRIMEIRA
SEGUNDA
> cat<saida.txt|tr a-z A-Z|wc -l
2
```

Também é possível usar aspas para nomes de arquivos com espaços ou para passar operadores como texto:

```text
> echo "a|b>c" > "nome com espacos.txt"
> cat < "nome com espacos.txt"
a|b>c
```

### Testes automatizados

No Linux, com GCC e Python 3 instalados, execute:

```bash
make test
```

A suíte contém 36 testes, incluindo `ls`, `cat` e `echo` com argumentos, `exit`, EOF, linha vazia, comando inexistente, acesso negado e os seguintes cenários:

- Erro real de leitura ao receber um diretório como entrada: mensagem na saída de erros e status de falha.
- Erro real de escrita ao enviar o prompt para `/dev/full`: mensagem na saída de erros e status de falha.
- Interrupções repetidas (`EINTR`) em `read()`, `write()` e `wait()`.
- Escritas parciais e escrita sem progresso do prompt.
- Falha de `wait()` e preservação da ordem entre o término do filho e o próximo prompt.
- Sobrescrita, acréscimo, leitura de arquivos e redirecionamentos combinados com pipelines.
- Pipelines com vários estágios, dados maiores que a capacidade do pipe e consumidores que encerram cedo.
- Falhas de `pipe()`, `dup2()` e `fork()`, incluindo cancelamento de um pipeline parcialmente iniciado.
- Interrupções repetidas em `open()` e `dup2()`.
- Aspas, operadores sem espaços, sintaxe inválida e rejeição dos limites excedidos.
- Uso repetido no mesmo processo, verificando a preservação dos descritores do pai, a ausência de vazamentos de descritores e a coleta dos filhos.

Os testes compilam um executável temporário e usam `--wrap` do linker para simular condições difíceis de provocar de forma determinística. Esse mecanismo não é incluído no executável normal gerado por `make`. A suíte não cobre todas as condições de erro possíveis.

## 8. Limitações conhecidas

- **Tamanho da entrada:** o buffer tem 1.024 bytes, dos quais até 1.023 são lidos por chamada; um byte fica reservado para `\0`. Uma linha maior não é rejeitada nem remontada, podendo ser processada em partes.
- **Limites por linha:** são aceitos até 16 comandos no pipeline, 63 argumentos por comando (contando o nome do programa) e 16 redirecionamentos por comando. A última posição de cada vetor de argumentos é reservada para `NULL`. Exceder esses limites gera erro de sintaxe, sem executar a linha.
- **Aspas e escapes:** aspas simples e duplas agrupam palavras e preservam operadores literais. Não há expansão de variáveis, substituição de comandos ou interpretação de escapes por barra invertida pelo shell. Aspas precisam ser fechadas na mesma entrada.
- **Leitura sem enquadramento de linhas:** cada bloco devolvido por `read()` é tratado como uma entrada completa. Dados recebidos por arquivo, pipe ou colagem de várias linhas podem ser agrupados ou divididos de forma inadequada. O uso previsto é interativo, com uma linha de comandos por vez.
- **Outros operadores:** não há suporte a execução em segundo plano (`&`), encadeamento (`;`, `&&`, `||`), here-documents (`<<`), redirecionamento de descritores específicos como `2>` ou expansão de curingas.
- **Comandos internos:** apenas `exit` é implementado. Não há `cd`, histórico, autocompletar ou controle de tarefas. `exit` isolado encerra mesmo se receber argumentos adicionais; não interpreta um código de saída informado pelo usuário e rejeita redirecionamentos. Em um pipeline, `exit` encerra apenas seu filho.
- **Sinais:** não há tratamento próprio de sinais. No terminal Linux usual, `Ctrl+C` pode encerrar o próprio shell pela ação padrão de `SIGINT`.
- **Estado de saída:** o estado retornado pelo filho é armazenado, mas não é analisado com macros como `WIFEXITED()` e `WEXITSTATUS()`. O shell não disponibiliza o resultado do último comando.

## 9. Demonstração e apresentação

[Vídeo de Demonstração]()

## 10. Referências

- Enunciado local: **UNIVERSIDADE FEDERAL DA BAHIA - Projeto.pdf**, páginas 1 e 2.
- GNU C Library: [criação de processos com `fork()`](https://sourceware.org/glibc/manual/latest/html_node/Creating-a-Process.html).
- GNU C Library: [execução de arquivos e família `exec`](https://sourceware.org/glibc/manual/latest/html_node/Executing-a-File.html).
- GNU C Library: [espera e término de processos](https://sourceware.org/glibc/manual/latest/html_node/Process-Completion.html).
- GNU C Library: [primitivas de entrada e saída](https://sourceware.org/glibc/manual/latest/html_node/I_002fO-Primitives.html).
- GNU C Library: [criação de pipes](https://sourceware.org/glibc/manual/latest/html_node/Creating-a-Pipe.html).
- GNU C Library: [duplicação de descritores com `dup2()`](https://sourceware.org/glibc/manual/latest/html_node/Duplicating-Descriptors.html).
- GNU C Library: [abertura e fechamento de arquivos](https://sourceware.org/glibc/manual/latest/html_node/Opening-and-Closing-Files.html).
