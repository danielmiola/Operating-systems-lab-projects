/* ================================================================== *
   Universidade Federal de Sao Carlos - UFSCar, Sorocaba

   Disciplina: Laboratorio de Sistemas Operacionais
   Prof. Gustavo Maciel Dias Vieira

   Projeto 3 - Módulos e Estruturas Internas do Núcleo

   Descricao: Módulo basico que imprime pid do processo e
              processo pai, alem de promover processo pai 
              a root.
   
   Daniel Ramos Miola          RA: 438340
   Giulianno Raphael Sbrugnera RA: 408093
* ================================================================== */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cred.h>


static int projeto3_show(struct seq_file *m, void *v) {
  //declara ponteiro para estrutura cred que será usada pra alterar as credenciais
  struct cred *credential;
  
  //imprime PID do processo atraves da macro current
  seq_printf(m, "current PID:%d\n",current->pid);

  //imprime PID do processo pai
  seq_printf(m, "parent  PID:%d\n",current->parent->pid);

  /*
  gera ponteiro mutavel para alteração, altera variaveis da 
  estrutura inserindo ID 0(root) e libera o ponteiro no fim
  da edição.
  */
  credential = (struct cred *) get_cred(current->parent->cred);
  
  //altera as variaveis relativas ao id de usuario na estrutura cred
  credential->uid = 0;    /* real UID of the task */
  credential->gid = 0;    /* real GID of the task */
  credential->suid = 0;   /* saved UID of the task */
  credential->sgid = 0;   /* saved GID of the task */
  credential->euid = 0;   /* effective UID of the task */
  credential->egid = 0;   /* effective GID of the task */

  put_cred(credential);

  return 0;
}

static int projeto3_open(struct inode *inode, struct file *file) {
     return single_open(file, projeto3_show, NULL);
}

static const struct file_operations projeto3_fops = {
     .owner	= THIS_MODULE,
     .open	= projeto3_open,
     .read	= seq_read,
     .llseek	= seq_lseek,
     .release	= single_release,
};

int init_module(void) {
  if (!proc_create("projeto3", 0644, NULL, &projeto3_fops)) {
    printk("Problema com o módulo!\n");
    return -ENOMEM;
  }
  printk("Módulo carregado!\n");
  return 0;
}

void cleanup_module(void) {
  remove_proc_entry("projeto3", NULL);
  printk("Módulo descarregado!\n");
}

MODULE_LICENSE("GPL");