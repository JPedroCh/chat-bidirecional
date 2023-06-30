# Projeto de pesquisa - Criando ambientes virtuais de conversação com uso system call select()

Disciplina: FGA0211 – Fundamentos de Redes de Computadores\
Semestre: 2023.1\
Curso: Engenharia de Software\
Professor: Fernando William Cruz\
Alunos: João Pedro Alves da Silva Chaves (180123203) e 
	      Lucas da Cunha Andrade (180105256)

## Como executar

Para compilar o código:

```bash
  gcc select.c -o select
```

Para executar o código:

```bash
  ./select <endereço IP> <porta>
```

Ex: 
```bash
  ./select 127.0.0.1 8000
```


## Funcionamento

### Servidor
Após iniciar a execução do programa, alguns comandos estão disponíveis para o servidor:
* **/list**: Lista as salas disponíveis com a quantidade atual de usuários na sala e o limite dela.
* **/users <id da sala>**: Lista os usuários de uma sala, porém é necessário enviar o id da sala.
* **/create**: Cria uma nova sala, esse comando é dividido em 2 etapas:
  * Definição do nome da sala
  * Definição do número máximo de usuários da sala
  * (Opcional) Definir uma senha para entrar na sala. Caso não precise de uma senha basta deixar vazio
* **/delete**: Exclui uma sala pelo id dela, o qual deve ser informado no passo seguinte ao comando.
  * Definição da sala a ser excluída
* **/exit**: Finaliza o programa que está sendo executado.
* **/help**: Lista os comandos disponíveis para o servidor.

### Cliente
Para conectar ao servidor basta executar:
```bash
  telnet <endereço IP> <porta>
```

Ex: 
```bash
  telnet 127.0.0.1 8000
```

Após a conexão é necessário informar o nome do usuário:
* **/name <nome do usuário>**: Define o nome do usuário conectado.
Após a definição os seguintes comandos se tornam disponíveis:
* **/list**: Lista as salas disponíveis com a quantidade atual de usuários na sala e o limite dela.
* **/join \<id da sala> \<senha>**: Troca o usuário de sala, sendo redirecionado para a sala indicada pelo id. Caso a sala escolhida possua senha é necessário informar a senha após o id da sala.
* **/leave**: Redireciona o usuário para a sala principal(lounge).
* **/exit**: Desconecta o cliente.
* **/help**: Lista os comandos disponíveis para o cliente/usuário.

Para se comunicar com outros usuários basta digitar a mensagem e clicar "enter". As mensagens enviadas por você não terão um nome de usuário ao lado esquerdo, porém as mensagens enviadas por outros usuários serão identificadas seguindo o padrão:

```bash
  joao: Oi
  Oi
  joao: Tudo bem?
  Bem e voce?
  joao: Bem
```

OBS: as mensagens só são enviadas para os usuários que estão na mesma sala.
