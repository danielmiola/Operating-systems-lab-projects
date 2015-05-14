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

int setorDisco;
int blocoDisco;
int fatAgrupamento; // quantos agrupamentos da fat sao necessarios armazená-la

unsigned short fat[65536];
int sizeLido[128];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];

// a função ceil apresentou erro, aqui fazemos a função teto
int teto(double numero) {

  int inteiro = numero;

  if(inteiro != numero) {
    return inteiro + 1;
  }

  return inteiro;

}

// escreve no disco de 4096 em 4096, ou seja, bloco em bloco
int escreve(int bloco, char* conteudo) {

  int i;

  for(i = 0 ; i < 8 ; i++) {

    if(!bl_write((bloco*8)+i, conteudo+(i*512))) {
      printf("Erro ao escrever no disco\n");
      return 0;
    }

  }

  return 1;


}

// le o disco de 4096 em 4096, ou seja, bloco em bloco
int le(int bloco, char* conteudo) {

  int i;

  for(i = 0 ; i < 8 ; i++) {

    if(!bl_read((bloco*8)+i,&conteudo[i*512])) {
      printf("Erro ao ler no disco\n");
      return 0;
    }

  }

  return 1;


}

int atualizaDiscoFat() {

  int i;
  char *buffer;
  buffer = (char*) fat;

  /*
   * PRECISAMOS COLOCAR 65536 POSICOES EM 32 AGRUPAMENTOS = 2048 POSIÇÕES POR AGRUPAMENTO
   * CADA AGRUPAMENTO TEM 8 SETORES = 256 POSIÇÕES POR AGRUPAMENTO
   * CADA SETOR TEM 512 BYTES, CADA POSIÇÃO TEM 2 BYTES -- cabem 256 posições em 512 bytes!
   */

  // armazena a fat no disco
  for(i = 0 ; i < fatAgrupamento ; i++) {

    if(!escreve(i,&buffer[i*4096])) {
      printf("Erro ao armazenar FAT no disco\n");
      return 0;
    }

  }

  buffer = (char*) dir;

  // armazena o diretorio no disco
  if(!escreve(i,buffer)) {
    printf("Erro ao armazenar diretório no disco\n");
    return 0;
  }

  return 1;

}

int fs_init() {

  setorDisco = bl_size();
  blocoDisco = setorDisco/8;
  fatAgrupamento = blocoDisco/2048;

  // verifica se o disco foi formatado -- carrega o fat

  int i;
  char* bufferFat = (char*) fat;

  for(i = 0 ; i < fatAgrupamento ; i++) {

    if(!le(i, &bufferFat[i*4096])) {
      return 0;
    }

  }

  char* bufferDir = (char*) dir;
  if(!le(i, bufferDir)) {
    return 0;
  }
  

  for(i = 0 ; i < fatAgrupamento ; i++) {

    if(fat[i] != 3) {

      printf("Disco não formatado!\n");
      return 1;

    } 

  }

  if(fat[fatAgrupamento] != 4) {

    printf("Disco não formatado!\n");
    return 1;

  }

  return 1;
}

int fs_format() {

  int i;

  for(i = 0 ; i < fatAgrupamento ; i++) {
    fat[i] = 3;
  }

  fat[i] = 4;

  for(i = fatAgrupamento+1 ; i < blocoDisco ; i++) {
    fat[i] = 1;
  }

  for(i = 0 ; i < 128 ; i++) {
    dir[i].used = 'N';
  }

  atualizaDiscoFat();

  return 1;
}

int fs_free() {

  int blocosLivres = 0;
  int i;

  for(i = 0 ; i < blocoDisco ; i++) {

    if(fat[i] == 1) {
   
      blocosLivres++;

    }

  }

  return blocosLivres*4096;
}

int fs_list(char *buffer, int size) {

  int i;
  char tamanho[10];

  strcpy(buffer, "");

  for(i = 0 ; i < 128 ; i++) { 

    if(dir[i].used != 'N') {
     sprintf(tamanho,"%d", dir[i].size);
      strcat(buffer, dir[i].name);
      strcat(buffer, "\t\t");
      strcat(buffer, tamanho);
      strcat(buffer, " bytes");
      strcat(buffer, "\n");
    }

  }

  return 1;
}

int fs_create(char* file_name) {

  int i;
  int livre = -1;

  for(i = 0 ; i < 128 ; i++) {

    // verifica se ha algum nome igual
    if(!strcmp(dir[i].name, file_name) && dir[i].used != 'N') {
      printf("Já existe um arquivo com esse nome\n");
      return 0;
    }

    // procura uma posicao livre
    if(livre == -1 && dir[i].used == 'N') {
      livre = i;
    }

  }

  if(livre == -1) {
    printf("Não há mais espaço\n");
    return 0;
  }

  // cria arquivo
  dir[livre].used = 'U';
  dir[livre].first_block = 2;
  strcpy(dir[livre].name, file_name);
  dir[livre].size = 0;

  atualizaDiscoFat();

  return 1;
}

void removeEntrada(int posicao) {

  // se posicao não é reservada
  if(fat[posicao] > 4) {
    removeEntrada(fat[posicao]);
  }

  fat[posicao] = 1;

}

int fs_remove(char *file_name) {

  int i = 0;
  
  while(i < 128 && (strcmp(dir[i].name,file_name) || dir[i].used == 'N')) {
    i++;
  }

  if(i == 128) {
    printf("Arquivo não existe ou está em uso\n");
    return 0;
  }

  dir[i].used = 'N';

  // remover posições da fat, se necessário
  if(dir[i].first_block != 2) {
    removeEntrada(dir[i].first_block);
  }

  atualizaDiscoFat();

  return 1;
}

int fs_open(char *file_name, int mode) {

  int i;

  // leitura
  if(mode == FS_R) {

    i = 0;

    // procura arquivo
    while(i < 128 && (strcmp(dir[i].name,file_name) || dir[i].used == 'N')) {
     i++;
    } 

    // não encontrou...
    if(i == 128) {
      printf("Arquivo não existe\n");
      return -1;
    }

    dir[i].used = 'R';
    sizeLido[i] = 0;

    // encontrou, retorne identificador
    return i;

  } else if(mode == FS_W) { // escrita

    i = 0;


    // procura arquivo
    while(i < 128 && (strcmp(dir[i].name,file_name) || dir[i].used == 'N')) {
     i++;
    }

    // não encontrou, crie o arquivo
    if(i == 128) {
      fs_create(file_name);
    } else { // encontrou! delete e crie um novo!
      fs_remove(file_name);
      fs_create(file_name);
    }

    i = 0;

    // procura onde o arquivo foi armazenado
    while(i < 128 && strcmp(dir[i].name,file_name)) {
     i++;
    }
    
    dir[i].used = 'W';

    // retorne identificador
    return i;

  }

  // modo inválido...
  printf("Modo inválido\n");
  return -1;
}

int fs_close(int file)  {
  
  if(dir[file].used == 'U' || dir[file].used == 'N') {
    printf("Impossível fechar arquivo!\n");
    return 0;
  }

  dir[file].used = 'U';

  atualizaDiscoFat();

  return 1;
}

int getBlocoLivre() {

  int i;

  for(i = 5 ; i < blocoDisco ; i++) {

    if(fat[i] == 1) {
      return i;
    }

  }

  return -1;

}

int fs_write(char *buffer, int size, int file) {

  int bloco, blocoNovo, tamanho, tamanhoLivreBloco, aux;
  int escrito = 0;
  int i = 0;
  char arquivoAberto[4096];

  if(file > 128 && dir[file].used != 'W') {
    return -1;
  }

  // verifica o tamanho do arquivo até agora
  tamanho = dir[file].size;

  // verifica o tamanho livre do ultimo bloco
  tamanhoLivreBloco = 4096 - tamanho%4096;

  // é preciso alocar espaço!
  if(tamanho == 0) {

    bloco = getBlocoLivre();
    fat[bloco] = 2;
    dir[file].first_block = bloco;

  } else { // arquivo já existe, qual é?

    bloco = dir[file].first_block;

    while(fat[bloco] != 2) {
      bloco = fat[bloco];
    }

    // deve-se escrever no último bloco
    if(!le(bloco, arquivoAberto)) {
      return -1;
    }

  }

  // escreve o arquivo todo
  while(escrito < size) {

    // o espaco livro no bloco é maior que o tamanho do arquivo
    if(tamanhoLivreBloco > size-escrito) {

      // insere novos dados ao bloco já existente
      for(i = 0 ; i < size-escrito ; i++) {
        arquivoAberto[4096-tamanhoLivreBloco+i] = buffer[i];
      }

      if(!escreve(bloco, arquivoAberto)) {
        return -1;
      }

    } else { // não cabe todo o arquivo no espaço livre do bloco

      // insere novos dados ao bloco já existente
      for(i = 0 ; i < tamanhoLivreBloco ; i++) {
        arquivoAberto[4096-tamanhoLivreBloco+i] = buffer[i];
      }

      aux = i;

      if(!escreve(bloco, arquivoAberto)) {
        return -1;
      }

      // necessário alocar outro bloco
      blocoNovo = getBlocoLivre();
      fat[bloco] = blocoNovo;
      fat[blocoNovo] = 2;
      bloco = blocoNovo; 
      tamanhoLivreBloco = 4096;

      for(i = aux ; i < size-escrito ; i++) {
        arquivoAberto[i-aux] = buffer[i];
      }

      tamanhoLivreBloco = size-escrito-i;

      if(!escreve(bloco, arquivoAberto)) {
        return -1;
      }

    }

    escrito += size-escrito;

  }

  dir[file].size += escrito;
  
  atualizaDiscoFat();

  return escrito;

}

int fs_read(char *buffer, int size, int file) {

  if(file > 128 && dir[file].used != 'R') {
    return -1;
  }

  char bufferAux[4096];
  int bloco = dir[file].first_block; // primeiro bloco
  int tamanho = dir[file].size; // tamanho do arquivo

  int sizeRestante = tamanho - sizeLido[file]; // tamanho do arquivo ainda não lido

  sizeRestante = sizeRestante < 0 ? 0 : sizeRestante;

  int leitura = sizeRestante < size ? sizeRestante : size; // qual é menor? -- para leitura
  int leituraOcorrida = 0;

  int blocoLeitura; // qual é o bloco que será lido
  int posicaoBlocoLeitura; // qual a posição dentro do bloco

  int i, aux;

  // procura o bloco...
  for(i = 0 ; i < (int) (sizeLido[file]/4096) ; i++) {
    bloco = fat[bloco];
  }

  while(leituraOcorrida < leitura) {

    blocoLeitura = sizeLido[file]/4096;
    posicaoBlocoLeitura = sizeLido[file]%4096;

    // o que será lido está num único bloco...
    if(4096-posicaoBlocoLeitura > leitura) {

      if(!le(bloco,bufferAux)) {
        return -1;
      }

      for(i = 0 ; i < leitura ; i++) {
        buffer[i] = bufferAux[i+posicaoBlocoLeitura];
      }

    } else { // o que será lido não está num único bloco

      // lê o que está no bloco...
      if(!le(bloco,bufferAux)) {
        return -1;
      }

      // lê o que tem no bloco
      for(i = 0 ; i < 4096-posicaoBlocoLeitura ; i++) {
        buffer[i] = bufferAux[i+posicaoBlocoLeitura];
      }

      aux = i;

      // procura o próximo bloco...
      bloco = fat[bloco];

      // lê o que está no bloco...
      if(!le(bloco,bufferAux)) {
        return -1;
      }

      // lê o que tem no bloco
      for(i = aux ; i < leitura-aux ; i++) {
        buffer[i] = bufferAux[i-aux];
      }

    }

    leituraOcorrida += leitura;
    sizeLido[file] += leitura;

  }

  return leitura;

}


