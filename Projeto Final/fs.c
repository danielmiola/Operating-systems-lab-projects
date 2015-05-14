/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096

unsigned short fat[65536];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];


//grava fat e dir da memoria no disco
int gravaFat(){

  int i;

  //grava fat no disco
  char * aux = (char*) fat;
  for(i = 0; i < 256; i++){
    if(!bl_write(i, &aux[i*512]))
      return 0;
  }

  //grava dir no disco
  aux = (char*) dir; 
  for(i = 0; i < 8; i++){
    if(!bl_write(i+256, &aux[i*512]))
      return 0;
  }

  return 1;
}

//função que remove arquivo marcando recursivamente os agrupamentos ocupados na fat
void removeFat(int agrupamento){

  if(fat[agrupamento] > 4){
    removeFat(fat[agrupamento]);
  }else{
    fat[agrupamento] = 1;
  }
}

/*
  Carrega FAT e DIR para a memoria e verifica se disco esta
  formatado com os valores corretos
*/
int fs_init() {
  
  int i;

  //referencia FAT e DIR para cast de leitura
  char* bufferFat = (char*) fat;
  char* bufferDir = (char*) dir;

  //le primeiros 256 setores contiguos do disco que correspondem a FAT
  for(i = 0; i < 256; i++){
    if(!bl_read(i, &bufferFat[i*512])){
      printf("Erro na leitura!\n");
      return 0;
    }
  }

  //le setor 257 onde se enconta o DIR
  for(i = 0; i < 8; i++){
    if(!bl_read(256+i, &bufferDir[i*512]))
      return 0;
  }

  //verifica se posições da FAT referentes a tabela estao com valores corretos
  for(i = 0; i < 32; i++){
    if(fat[i] != 3){
      printf("Disco nao formatado!\n");
      return 1;
    }
  }

  //verifica se posicao da FAT referente a DIR esta com valor correto
    if(fat[32] != 4){
      printf("Disco nao formatado!\n");
      return 1;
  }

  return 1;
}

/*
  Atualiza valores na tabela FAT e em DIR para todos os blocos livres
  e grava FAT e DIR no disco
*/
int fs_format() {
  
  int i;

  //atualiza valor de todas as posicoes referentes a FAT
  for(i = 0; i < 32; i++){
    fat[i] = 3;
  }

  //atualiza valor referente a DIR
  fat[32] = 4;

  //atualiza todas as outras posições como livres
  for(i = 33; i < 65536; i++){
    fat[i] = 1;
  }

  //atualiza valor de todas as posicoes do diretorio como livres
  for(i = 0; i < 128; i++){
    dir[i].used = 'F';
  }

  if(!gravaFat()){
    printf("Erro ao gravar no disco!\n");
  }

  return 1;
}

//varre tabela contando o numero de blocos livres
int fs_free() {

  int disco, livre = 0, i;
  //numero max de blocos para o disco atual
  disco = bl_size()/8;

  //calcula numero de blocos livres pela fat e retorna tamanho em bytes
  for (i = 0; i < disco; i++){
    if(fat[i] == 1)
      livre++;
  }

  return livre*4096;
}

//varre diretorio concatenando os atributos dos arquivos em uso
int fs_list(char *buffer, int size) {
  
  int i;
  strcpy(buffer, "");
  char tamanho[10];

  for(i = 0; i < 128; i++){
    if(dir[i].used != 'F'){        
      sprintf(tamanho,"%d",dir[i].size);

      strcat(buffer, dir[i].name);
      strcat(buffer, "\t\t");
      strcat(buffer, tamanho);
      strcat(buffer, "\n");
    }
  }

  strcat(buffer, "\0");

  return 1;
}

//varre estruturas procurando se o arquivo ja existe, caso nao
//cria suas referencias tendo em vista que exite espaço livre
int fs_create(char* file_name) {
  
  int i, file = -1;

  for(i = 0; i < 128 ; i++){

    //procura em DIR arquivo com mesmo nome que esteja em uso
    if(!strcmp(dir[i].name, file_name) && dir[i].used == 'T'){
      printf("Arquivo ja existente\n");
      return 0;
    }
  }

  for(i = 0; i < 128 ; i++){

    //procura em DIR primeiro espaço livre
    if(dir[i].used == 'F'){
      file = i;
      break;
    }
  }    

  //verifica se foi encontrado espaço livre
  if(file == -1){
    printf("Disco cheio!\n");
    return 0;
  }

  //acha primeiro agrupamento livre para arquivo
  int agrupamento = -1;
  int disco = bl_size()/8;
  for(i = 33; i < disco; i++){
    if(fat[i] == 1){
      agrupamento = i;
      break;
    }
  }

  if(agrupamento == -1){
    printf("Disco cheio!\n");
    return 0;
  }

  //atualiza valores em DIR
  dir[file].used = 'T';
  strcpy(dir[file].name, file_name);
  dir[file].first_block = agrupamento;
  dir[file].size = 0;

  //atualiza valores em fat
  fat[agrupamento] = 2;

  if(!gravaFat()){
    printf("Erro ao gravar no disco!\n");
  }

  return 1;
}

//varre diretório atras do arquivo requisitado e chama funcao recursiva que
//apaga referencias dos agrupamentos da tabela de alocação
int fs_remove(char *file_name) {
  
  int i, arq = -1;

  for(i = 0; i < 128; i++){
  
    if(!strcmp(dir[i].name,file_name) && (dir[i].used == 'T')){
      arq = i;
      break;
    }
  }

  if(arq == -1){
    printf("Arquivo não existe\n");
  } else {
    //marca arquivo como livre em diretorio
    dir[i].used = 'F';
    //chama funcao recursiva para remoção casa arquivo possua mais de um agrupamento
    removeFat(dir[i].first_block);
  }

  gravaFat();

  return 1;
}

int fs_open(char *file_name, int mode) {
  
  int i, arq = -1;

  //abre arquivo para leitura
  if(mode == FS_R){

    //procura o arquivo para leitura
    for(i = 0; i < 128; i++){
  
      if(!strcmp(dir[i].name,file_name) && (dir[i].used == 'T')){
        arq = i;
        break;
      }
    }

    if(arq == -1){
      printf("Arquivo não existe\n");
      return -1;
    }

    //marca arquivo em modo R(read) e retorna identificador
    dir[i].used = 'R';
    return i;

  }else if(mode == FS_W){

    arq = -1;
    //procura o arquivo existente
    for(i = 0; i < 128; i++){
  
      if(!strcmp(dir[i].name,file_name) && (dir[i].used == 'T')){
        arq = i;
        break;
      }
    }

    //se arquivo nao e encontrado
    if(arq == -1){

      //cria arquivo
      fs_create(file_name);
        
      //procura o arquivo para identificador
      for(i = 0; i < 128; i++){  
        if(!strcmp(dir[i].name,file_name) && (dir[i].used == 'T')){
          arq = i;
          break;
        }
      }

      //marca como W(write) e retorna identificador
      dir[i].used = 'W';
      gravaFat();
      return i; 
        
    }else{

      //apaga e cria arquivo com tamanho 0
      fs_remove(file_name);
      fs_create(file_name);

      //procura o arquivo para identificador
      for(i = 0; i < 128; i++){  
        if(!strcmp(dir[i].name,file_name) && (dir[i].used == 'T')){
          arq = i;
          break;
        }
      }

      //marca como W(write) e retorna identificador
      dir[i].used = 'W';
      gravaFat();
      return i; 
    }

  }

  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}

