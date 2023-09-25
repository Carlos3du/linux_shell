//BIBLIOTECAS INCLUÍDAS NO PROJETO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

//Definindo tamanho máximo dos comandos
#define MAX_LEN 1000

//Struct para armazenar os comandos que serão executados no shell
typedef struct Command{ 
    char comando[MAX_LEN];
    struct Command *next;
}Command;

//FUNÇÕES DE INPUT E FORMATAÇÃO DA ENTRADA
void print_prompt();
void verify_user_input(Command **head, Command **tail, char *user_in);
void insert_cmd(Command **head, Command **tail, char *cmd);
void remove_spaces(char *str);

//FUNÇÕES DE EXECUÇÃO SEQUENCIAL PADRÃO
void exec_command(char *cmd);
void sequential_exec(Command **head, Command **tail);

//FUNÇÕES DE PIPE NO MODO SEQUENCIAL
void pipe_commands_seq(char *cmd1, char *cmd2);

//FUNÇÕES DE EXECUÇÃO PARALELA PADRÃO
void *read_cmd(void *command);
void parallel_exec(Command **head, Command **tail);

//FUNÇÕES DE PIPE NO MODO PARALELO
void *pipe_out_thread(void *cmd);
void *pipe_in_thread(void *cmd);
void pipe_commands_par(char *cmd1, char *cmd2);

//FUNÇÕES DE REDIRECIONAMENTO DE ENTRADA E SAÍDA NO MODO PARALELO
void *output_append_par(void *cmd, void *input_f);
void *output_write_par(void *cmd, void *input_f);
void *input_write_par(void *program, void *input_f);

//FUNÇÕES DE REDIRECIONAMENTO DE ENTRADA E SAÍDA NO MODO SEQUENCIAL
void output_append_seq(char *command, char *filename);
void output_write_seq(char *command, char *filename);
void input_write_seq(char *program, char *input_f) ;

//VARIÁVEIS GLOBAIS
char last_cmd[MAX_LEN]; //variavel pra armazenar o ultimo comando armazenado
int style = 0; //0 = seq, 1 = par
int type = 0; //0 = int //1 = batch_file
int exit_flag = 0; //flag para sinalizar a saída do programa
int pipe_file[2]; //variável para armazenar a entrada e saída do pipe

/*
    FUNÇÃO MAIN QUE CHAMA OUTRAS FUNÇÕES PARA EXECUTAR OS PROCESSOS E THREADS
*/

int main(int argc, char *argv[]){
    char *user_input = NULL;
    size_t length = 0;
    ssize_t read;
    FILE *file;

    Command *head = NULL, *tail = NULL;

    if(argc == 2){ //Se iniciado o shell com um argumento, ele executa o shell no modo BATCH
        type = 1;

        char *file_name = argv[1];
        file = fopen(file_name, "r");
    }
    while(1){

        if(type == 0){
            print_prompt();
        }

        if(type == 1 && file == NULL){
            perror("Arquivo nao encontrado");
            return 1;
        }
        
        if(type == 1){//lendo a linha de um arquivo
            read = getline(&user_input, &length, file);
            
        }
        else{//lendo a linha no modo interativo
            read = getline(&user_input, &length, stdin);
        }
        
        if((read == (ssize_t)-1)){ //saindo caso o "ctrl+d" ou EOF
            break;
        }
        
        if((read > 0 && user_input[read - 1] == '\n')){//tirando a quebra de linha
            user_input[read - 1] = '\0';
        }        

        //Definindo se o estilo será paralelo ou sequencial na hora da execução, só vai printar se o modo for interativo
        if (strcmp(user_input,"style parallel") == 0 || strstr(user_input, "style parallel") != NULL){ 
            style = 1;

            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }

        }
        else if(strcmp(user_input,"style sequential") == 0 || strstr(user_input, "style sequential") != NULL){
            style = 0;

            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }
        }
        else{
            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }

            verify_user_input(&head, &tail, user_input);//Separando os comandos pelo ";"

            if(style == 0){
                sequential_exec(&head, &tail); //execucao sequencial
                
            }
            else if(style == 1){
                parallel_exec(&head, &tail); //execução paralela

                if (exit_flag == 1){
                    sleep(1);
                    return 0;
                }
            }
        }

        if(type == 1){
            free(user_input); //dando free na linha lida caso seja o modo BATCH
        }
    }

    if (type == 1){ //fechando o arquivo caso seja no modo BATCH
       fclose(file);
    }
    return 0;
}

/*
    FUNÇÕES DE INPUT E FORMATAÇÃO
*/

void print_prompt(){//Função de printar o prompt "cepc (style)>"
    if(style == 1 && type == 0){
        printf("cepc par> ");
    }
    else if(style == 0 && type == 0){
        printf("cepc seq> ");
        style = 0;
    }
}

void verify_user_input(Command **head, Command **tail, char *user_in){//separando os comandos por ";"
    char *cmd;

    cmd = strtok(user_in, ";");//separando os comandos pelo caractere ";"

    while(cmd != NULL){
        remove_spaces(cmd);
        insert_cmd(head, tail, cmd);
        cmd = strtok(NULL, ";");
    }
}

void insert_cmd(Command **head, Command **tail, char *cmd){//inserir comando na fila
    Command *new = (Command *)malloc(sizeof(Command));

    if(new != NULL){
        if(cmd[0] != 0 && strcmp(cmd, "!!") != 0){
            strcpy(last_cmd, cmd);//determinando o ultimo comando
        }
        if(strcmp(cmd, "!!") == 0){ //se o comando for "!!", ele vai inserir o último comando caso tenha 
            if(strlen(last_cmd) <= 0){
                printf("Nao foi inserido nenhum comando anteriormente\n");
            }
            else{
                strcpy(new->comando, last_cmd);
            }
        }
        else{
            strcpy(new->comando, cmd);
        }
        new->next = NULL;
        if(*head == NULL){
            *head = new;
            *tail = new;
        }
        else{
            (*tail)->next = new;
            *tail = new;
        }
    } 
}

void remove_spaces(char *str){//Remover os espaços do comandos
    int len = strlen(str);
    int start = 0, j = 0;
    int end = (len - 1);

    //Encontrando a posição do primeiro caractere que não é "espaço"
    while(str[start] == ' ' || str[start] == '\t'){
        start++;
    }

    while(end > start && (str[end] == ' ' || str[end] == '\t')){//Encontrando o index do último char sem ser "espaço"
        end--;
    }

    for(int i = start; i <= end; i++){ //Movendo os caracteres sem ser "espaço" pro inicio da string
        str[j++] = str[i];
    }

    str[j] = '\0'; 
}

/*
    MODO SEQUENCIAL E SUAS FUNÇÕES UTILIZADAS
*/

void exec_command(char *cmd){ //executando os comandos no modo sequencial ou em casos de exeções
    char *args[MAX_LEN]; //Array de argumentos + o NULL no final
    int arg_count = 0;

    if(strcmp(cmd, "exit") == 0){
        exit(0);
    }
    
    //Divide a entrada em tokens usando espaço 
    char *token = strtok(cmd, " ");

    while (token != NULL) {
        args[arg_count] = token;
        arg_count++;
        token = strtok(NULL, " ");
    }

    args[arg_count] = NULL; //Coloca NULL no final do array de argumentos

    //Executa o comando usando seus argumentos
    execvp(args[0], args);  
    perror("Comando invalido - erro");
}

void sequential_exec(Command **head, Command **tail){//Executando sequencialmente
    Command *temp, *aux = *head;
    pid_t child_pid;
    int child_status;

    while(aux != NULL){
        child_pid = fork(); //criando processo filho
     
        if(child_pid == 0){ //processo filho  
            if(type == 1){
                printf("%s\n", aux->comando);//printando o comando antes de executa-lo
                sleep(1);
            }
            if(strchr(aux->comando, '|') != NULL) { //executar os comandos em pipe
                char *cmd1 = strtok(aux->comando, "|");
                char *cmd2 = strtok(NULL, "|");

                remove_spaces(cmd1);
                remove_spaces(cmd2);
                                       
                pipe_commands_seq(cmd1, cmd2);
            }
            else if(strstr(aux->comando, ">>") != NULL){//Output com Apend em um arquivo (>>) 
          
                char *cmd = strtok(aux->comando, ">>");
                char *file_name = strtok(NULL, ">>");

                remove_spaces(cmd);
                remove_spaces(file_name);
                
                output_append_seq(cmd, file_name);
                exit(0);
            }
            else if(strchr(aux->comando, '>') != NULL){ //Output em um arquivo (>)
                char *cmd = strtok(aux->comando, ">");
                char *file_name = strtok(NULL, ">");

                remove_spaces(cmd);
                remove_spaces(file_name);

                output_write_par(cmd, file_name);
                exit(0);
            }
            else if(strchr(aux->comando, '<') != NULL){ //input vindo de um arquivo (<)
                char *prog = strtok(aux->comando, "<");
                char *file_name = strtok(NULL, "<");

                remove_spaces(prog);
                remove_spaces(file_name);

                input_write_seq(prog, file_name);
                exit(0);
            }
            else{//executando da maneira padrão
                remove_spaces(aux->comando);
                exec_command(aux->comando);
                exit(0);
            }
        } 
        else if(child_pid < 0){ 
            perror("Criação do processo filho - erro");
        }
        else{
            if(strstr(aux->comando, "exit") != NULL){
                sleep(1);
                exit(0);
            }
            else{
                waitpid(child_pid, &child_status, 0);
            }
            aux = aux->next; 
        }
        
    }

    while(*head != NULL){ //limpando os comandos depois de executar todas as threads
        temp = *head;
        *head = (*head)->next;
        free(temp);
    }
    *tail = NULL;
}

void pipe_commands_seq(char *cmd1, char *cmd2){//executando os comandos no modo pipe, no estilo sequencial
    int pipefd[2]; 
    pid_t pipe_pid;

    if(pipe(pipefd) != 0){ //criando um pipe
        perror("Criacao pipe - erro");
    }

    pipe_pid = fork();
    
    if(pipe_pid < 0){
        perror("Criacao processo pipe - erro");
    }
    else if(pipe_pid == 0){  //processo neto
        close(pipefd[1]); //fecha a extremidade de escrita do pipe

        //redireciona a entrada do cmd1 pra a leitura do pipe 
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);  //fechando a extremidade de leitra do pipe

        exec_command(cmd2);

        perror("Pipe cmd 2 -erro"); 

        exit(1);
    } 
    else{ //processo filho
        close(pipefd[0]); 
       
        dup2(pipefd[1], STDOUT_FILENO); //lê a entrada do cmd1 pra executa no cmd2
        close(pipefd[1]);  //Fecha a extremidade de escrita do pipe
 
        exec_command(cmd1);

        perror("Pipe cmd 1 -erro");
        exit(1);
    }
}

void output_append_seq(char *cmd, char *filename){ //Output com Apend em um arquivo, modo sequencial
    FILE *file = fopen(filename, "a"); //Abre o arquivo em modo de append

    if(file == NULL){
        perror("Erro ao abrir o arquivo");
    }

    //Redireciona a saída para o arquivo
    if(dup2(fileno(file), STDOUT_FILENO) == -1){
        perror("dup2 - erro");
        fclose(file);
        
    }

    //Executa o comando e redireciona sua saída para o arquivo
    if(system(cmd) == -1){
        perror("Append no arquivo seq; Comando invalido - erro");
        fclose(file);  
    }

    fclose(file);
}

void output_write_seq(char *cmd, char *filename){//Output sendo escrito/sobreescrevendo um arquivo
    FILE *file = fopen(filename, "w+");
    
    if(file == NULL){
        perror("Output write; Falha ao abrir o arquivo - erro");
    }

    //Redirecionar a saída para o arquivo
    if(dup2(fileno(file), STDOUT_FILENO) == -1){
        perror("dup2 - erro");
    }

    //Executar o comando redirecionar sua saída para o arquivo
    if(system(cmd) == -1){
        perror("Output write; Comando invalido - erro");
    }

    fclose(file);
}

void input_write_seq(char *program, char *input_f){//Input vindo de um arquivo, modo sequencial
    int pid;
    int status;

    pid = fork(); 

    if(pid == -1){
        perror("Input write seq; Falha ao criar um processo - erro");
    }

    if(pid == 0){ //Processo filho
        FILE *file = fopen(input_f, "r");
        if(file == NULL){
            perror("Input write seq; Falha em abrir o arquivo - erro");
            exit(1);
        }
       
        if(dup2(fileno(file), STDIN_FILENO) == -1){ //Redireciona a entrada para o arquivo
            perror("dup2 - erro");
            fclose(file);
            exit(1);
        }

        fclose(file);

        //Executa o programa
        exec_command(program);
        perror("Input write seq; Comando invalido - erro");
        exit(1);
    }
    else{ 
        wait(&status);
    }
}

/*
    MODO PARALELO E SUAS FUNÇÕES UTILIZADAS
*/

void *read_cmd(void *command){ //tratando o ponteiro void e executando o comando no sistema de maneira paralela
    char *read = (char *)command;

    if(strcmp(read, "exit") == 0){
        exit_flag = 1;
        pthread_exit(NULL);
    }
    else if(strchr(read, '|') != NULL) { //Executando em pipe
    char *cmd1 = strtok(read, "|");
    char *cmd2 = strtok(NULL, "|");

    remove_spaces(cmd1);
    remove_spaces(cmd2);

    pipe_commands_par(cmd1, cmd2);
    }
    else if(strstr(read, ">>") != NULL){ //Output com Apend em um arquivo (>>) 
        char *cmd = strtok(read, ">>");
        char *file_name = strtok(NULL, ">>");

        remove_spaces(cmd);
        remove_spaces(file_name);

        output_append_par(cmd, file_name);
    }
    else if(strstr(read, ">") != NULL){ //Output em um arquivo (>)
        char *cmd = strtok(read, ">");
        char *file_name = strtok(NULL, ">");

        remove_spaces(cmd);
        remove_spaces(file_name);

        output_write_par(cmd, file_name);
    }
    else if(strstr(read, "<") != NULL){ //input vindo de um arquivo (<)
        char *cmd = strtok(read, "<");
        char *file_name = strtok(NULL, "<");

        remove_spaces(cmd);
        remove_spaces(file_name);

        input_write_par(cmd, file_name);
    }
    else{
        if(system(read) != 0){//executando o comando de maneira padrão
            perror("Comando invalido - erro");
        }
        pthread_exit(NULL);
    }
    return NULL;
}

void parallel_exec(Command **head, Command **tail){//executar em paralelo os comandos, em forma de threads
    
    pid_t child_pid;
    int child_status;
    Command *aux = *head;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)); //criado um "array" de threads
    int count = 0;

    child_pid = fork();

    if(child_pid == 0){//processo filho
        while(aux != NULL){
            if (threads == NULL){
                perror("Alocacao de memoria na criacao de thread - erro");
                break;
            }
            else{
                if(pthread_create(&threads[count], NULL, read_cmd, (void*)aux->comando) != 0){
                    perror("Criacao da thread - erro");
                    break;
                }
                count++; //Contando o número total de threads
                
                threads = (pthread_t *)realloc(threads, (count + 1) * sizeof(pthread_t));//realocando o espaço de memoria das threads 

                if(threads == NULL){
                    perror("Realoc de memoria para as threads - erro");    
                }
            }

            if (exit_flag == 1){
                break;
            }

            aux = aux->next;
        }
        
        for(int i = 0; i < count; i++){
            if(pthread_join(threads[i], NULL) != 0){ //finalizando todas as threads depois de executa-llas em uma ordem aleatória
                perror("Espera por threads - erro");  
            }
        }

        exit(0);
    }
    else if(child_pid < 0){
        perror("Criação do processo filho - erro");
    }
    else{     
        waitpid(child_pid, &child_status, 0);
    }

    Command* temp;

    while(*head != NULL){ //limpando os comandos depois de executar todas as threads
        temp = *head;
        if(strcmp(temp->comando, "exit") == 0){
            exit_flag = 1;
        }
        
        *head = (*head)->next;
        free(temp);
    }
    *tail = NULL;

    free(threads); //limpando o array de threads    
    
}


void *pipe_out_thread(void *cmd){ //pipe de saída no estilo paralelo
    char *cmd1 = (char *)cmd;
    close(pipe_file[0]); //Fecha a extremidade de leitura do pipe

    //Redireciona a saída padrão para a extremidade de escrita do pipe
    dup2(pipe_file[1], STDOUT_FILENO);
    close(pipe_file[1]); //Fecha a extremidade de escrita do pipe

    if(system(cmd1) != 0){
        perror("Pipe out thread; Comando invalido - erro");
    }

    sleep(1);
    pthread_exit(NULL);
}

void *pipe_in_thread(void *cmd){//extremidade de entrada no estilo paralelo
    char *cmd2 = (char *)cmd;
    close(pipe_file[1]); //fecha a extremidade de escrita do pipe

    //Redireciona a para a extremidade de leitura do pipe
    dup2(pipe_file[0], STDIN_FILENO);
    close(pipe_file[0]); //Fecha a extremidade de leitura do pipe

    if(system(cmd2) != 0){
        perror("Pipe in thread; Comando invalido - erro");
    }

    sleep(1);
    pthread_exit(NULL);
}

void pipe_commands_par(char *cmd1, char *cmd2){//Executando os comandos no estilo paralelo em pipe

    if(pipe(pipe_file) == -1){
        perror("Criacao do pipe - erro");
        exit(1);
    }

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, pipe_in_thread, (void*) cmd2);
    pthread_create(&thread2, NULL, pipe_out_thread, (void*) cmd1);

    sleep(1);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
}

void *output_append_par(void *cmd, void *input_f){//Output com Apend em um arquivo, modo paralelo

    char *cmd1 = (char *)cmd;
    char *filename = (char *)input_f;

    FILE *file = fopen(filename, "a"); //Abre o arquivo em modo append

    if(file == NULL){
        perror("Falha ao abrir o arquivo - erro");
        pthread_exit(NULL);
    }

    //Redireciona a saída padrão para o arquivo
    if(dup2(fileno(file), STDOUT_FILENO) == -1){
        perror("dup2 - erro");
        fclose(file);    
    }

    //Executa o comando e redireciona sua saída para o arquivo
    if(system(cmd1) == -1){
        perror("Output_append_par; comando invalido - erro");
        fclose(file);  
    }

    fclose(file);
    pthread_exit(NULL);
}

void *output_write_par(void *cmd, void *input_f) { 
    
    char *cmd1 = (char *)cmd;
    char *filename = (char *)input_f;

    FILE *file = fopen(filename, "w+");
    if (file == NULL) {
        perror("open");
    }

    //Redirecionar a saída para o arquivo
    if (dup2(fileno(file), STDOUT_FILENO) == -1) {
        perror("dup2");
    }

    //Executar o comando e redirecionar sua saída para o arquivo
    if (system(cmd1) == -1) {
        perror("system");
    }

    fclose(file);

    pthread_exit(NULL);
}

void *input_write_par(void *program, void *input_f){//Usar um arquivo como input no modo paralelo 
    char *prog = (char *)program;
    char *file_name = (char *)input_f;
    int pid;
    int status;

    pid = fork(); 

    if(pid == -1){
        perror("Criacao do processo filho - erro");
    }

    if(pid == 0){ //Processo filho
        FILE *file = fopen(file_name, "r");
        if(file == NULL){
            perror("Falha ao abrir o arquivo - erro");
            pthread_exit(NULL);
        }

        //Redireciona a entrada para o arquivo
        if(dup2(fileno(file), STDIN_FILENO) == -1){
            perror("dup2 - erro");
            fclose(file);
            pthread_exit(NULL);
        }

        fclose(file);

        exec_command(prog);
        perror("Comando invalido - erro");
        pthread_exit(NULL);
    }
    else{ 
        wait(&status);
        pthread_exit(NULL);
    }
}





