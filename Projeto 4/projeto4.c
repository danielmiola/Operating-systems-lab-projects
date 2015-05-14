/* ================================================================== *
   Universidade Federal de Sao Carlos - UFSCar, Sorocaba

   Disciplina: Laboratorio de Sistemas Operacionais
   Prof. Gustavo Maciel Dias Vieira

   Projeto 4 - Programacao Concorrente

   Descricao: tornar o programa de quebrar senhas multithreaded, 
              aumentando a sua eficiencia.
   
   Daniel Ramos Miola          RA: 438340
   Giulianno Raphael Sbrugnera RA: 408093
* ================================================================== */

// bibliotecas do sistema
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <crypt.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

// definicao dos tamanhos maximos das senhas, da fila circular e do tamanho da hash
#define TAM_SENHA 4
#define N_ITENS   50
#define TAM_HASH  256

// Variaveis globais que facilitam o compartilhamento de dados
// fila circular compartilhada pelas threads
char *shared_passwd[N_ITENS];

// senha que sera testada e incrementada caso necessario 
char senha[TAM_SENHA + 1];

// ponteiro para char que guarda o valor da hash fornecida
const char *target;

// definem o ultimo elemento consumido e o ultimo elemento produzido, respectivamente
int inicio = 0, final = 0;

sem_t bloqueio, pos_vazia, pos_ocupada;

/* Obtém o hash a partir de uma senha e coloca o resultado em hash.
   O vetor hash deve ter pelo menos 14 elementos. */
void calcula_hash_senha(const char *senha, char *hash);

void incrementa_senha(char *senha);

void testa_senha(const char *hash_alvo, const char *senha);

void* produtor(void *v);

void* consumidor(void *v);

int main(int argc, char *argv[]) 
{
    int i;

    // Conjunto de threads que serao utilizadas ao longo do programa
    pthread_t thr_produtor;
    pthread_t thr_consumidor1;
    pthread_t thr_consumidor2;
    pthread_t thr_consumidor3;

    if (argc != 2) 
    {
        printf("Uso: %s <hash>", argv[0]);
        return 1;
    }

    // Inicializacao da senha com a sequencia 'aaaa'
    for (i = 0; i < TAM_SENHA; i++) 
    {
        senha[i] = 'a';
    }
    senha[TAM_SENHA] = '\0';

    // Guarda em target a hash a ser comparada durante a execucao
    target = argv[1];

    // Inicializa os semaforos
    sem_init(&bloqueio, 0, 1);
    sem_init(&pos_vazia, 0, N_ITENS);
    sem_init(&pos_ocupada, 0, 0);

    // Limpa a fila circular
    for (i = 0; i < N_ITENS; i++)
    {
        shared_passwd[i] = NULL;
    }

    pthread_create(&thr_produtor, NULL, produtor, NULL);
    pthread_create(&thr_consumidor1, NULL, consumidor, NULL);
    pthread_create(&thr_consumidor2, NULL, consumidor, NULL);
    pthread_create(&thr_consumidor3, NULL, consumidor, NULL);

    pthread_join(thr_produtor, NULL); 
    pthread_join(thr_consumidor1, NULL);
    pthread_join(thr_consumidor2, NULL);
    pthread_join(thr_consumidor3, NULL);

    return 0;
}

void testa_senha(const char *hash_alvo, const char *senha) {
  char hash_calculado[TAM_HASH + 1];

  calcula_hash_senha(senha, hash_calculado);
  if (!strcmp(hash_alvo, hash_calculado)) {
    printf("Achou! %s\n", senha);
    exit(0);
  }
}

void incrementa_senha(char *senha) {
  int i;

  i = TAM_SENHA - 1;
  while (i >= 0) {
    if (senha[i] != 'z') {
      senha[i]++;
      i = -2;
    } else {
      senha[i] = 'a';
      i--;
    }
  }
  if (i == -1) {
    printf("Não achou!\n");
    exit(1);
  }
}


void calcula_hash_senha(const char *senha, char *hash) {
  struct crypt_data data;
  data.initialized = 0;
  strcpy(hash, crypt_r(senha, "aa", &data));
}

// Funcao onde o produtor opera
// Aqui a senha vai ser adicionada no final da fila circular
// Depois incrementada
// E o final da fila circular sera incrementado, respeitando o limite pre-estabelecido
// Isto significa que um item, no caso uma senha, foi produzido
void* produtor(void *v) 
{
    while (1)
    {
        sem_wait(&pos_vazia);
        sem_wait(&bloqueio);

        shared_passwd[final] = senha;     
        incrementa_senha(senha);
        final = (final + 1) % N_ITENS;  
    
        sem_post(&bloqueio);
        sem_post(&pos_ocupada);
    }
    
    return NULL;
}

// Funcao onde o consumidor opera
// Aqui a senha presente no inicio da fila circular vai ser copiada para a senha
// O inicio da fila circular sera incrementado, respeitando o limite pre-estabelecido
// Isto representa que um item, no caso uma senha, foi consumido
// Depois a senha sera testada
// Ha ainda uma variavel auxilixar que sera usada para o teste fora da região critica
// Isso se faz necessario para que varios consumidores nao testem uma mesma senha
void* consumidor(void *v) 
{
    char senha_aux[TAM_SENHA + 1];

    while (1)
    {
        sem_wait(&pos_ocupada);
        sem_wait(&bloqueio);

        strcpy(senha_aux, shared_passwd[inicio]);
        inicio = (inicio + 1) % N_ITENS;

        sem_post(&bloqueio);
        sem_post(&pos_vazia);
 
        testa_senha(target, senha_aux);
    }
   
    return NULL;
}