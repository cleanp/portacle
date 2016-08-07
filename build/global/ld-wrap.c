#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int (*o_execve)(const char *filename, char *const argv[], char *const envp[]) = NULL;

// Returns a fresh array of strings to use as the proper argv
// to our ld-linux.so call.
char **ld_wrap_argv(const char *filename, char *const argv[]){
  int len;
  for(len=0; argv[len]; ++len);
  
  char **argv_t = calloc(3+len, sizeof(char *));
  argv_t[0] = "ld-linux.so";
  argv_t[1] = "--library-path";
  argv_t[2] = getenv("LW_LIBRARY_PATH");
  argv_t[3] = (char *)filename;
  for(int i=1; argv[i]; ++i){
    argv_t[i+3] = argv[i];
  }
  return argv_t;
}

// Simply prints a debug message for the filename and args.
void ld_wrap_log(const char *filename, char *const argv[]){
  fprintf(stderr, "LD_WRAP: %s", filename);
  for(int i=1; argv[i]; ++i)
    fprintf(stderr, " %s", argv[i]);
  fprintf(stderr, "\n");
}

// Returns a fresh copy of the search path to use.
char *ld_wrap_path(){
  char *path = getenv("PATH");
  if(path == NULL){
    size_t len = confstr(_CS_PATH, (char *)NULL, 0);
    char *path = calloc(2+len, sizeof(char));
    path[0] = ':';
    confstr(_CS_PATH, path+1, len);
    return path;
  }else{
    return strdup(path);
  }
}

// Returns true if the given filename exists, is a
// file and has the executable bit set.
int ld_wrap_exe_p(char *filename){
  struct stat st;
  return (stat(filename, &st) == 0)
    && S_ISREG(st.st_mode) 
    && (S_IXUSR & st.st_mode);
}

// Attempt to resolve the filename to an absolute one
// using PATH and friends.
char *ld_wrap_resolv(const char *filename){
  char *resolved = (char *)filename;
  if(!strchr(filename, '/')){
    char *path = ld_wrap_path();
    size_t pathlen = strlen(path);
    size_t filelen = strlen(filename);
    char *current_path = calloc(pathlen+filelen+2, sizeof(char));
    
    if(current_path != NULL){
      size_t j=0;
      for(size_t i=0; i<pathlen; ++i){
        if(path[i] == ':'){
          current_path[j] = '/';
          for(int k=0; k<filelen; ++k) current_path[j+1+k]=filename[k];
          current_path[j+filelen+1] = 0;
          if(ld_wrap_exe_p(current_path)){
            resolved = current_path;
            break;
          }else{
            j=0;
          }
        }else{
          current_path[j]=path[i];
          ++j;
        }
      }
    }
    free(path);
  }
  return resolved;
}

int execve(const char *filename, char *const argv[], char *const envp[]){
  o_execve = o_execve ? o_execve : dlsym(RTLD_NEXT, "execve");
  const char *loader = getenv("LW_LOADER_PATH");
  char **argv_t = ld_wrap_argv(filename, argv);
  ld_wrap_log(loader, argv_t);
  int status = o_execve(loader, argv_t, envp);
  free(argv_t);
  return status;
}

int execv(const char *filename, char *const argv[]){
  char *const envp[] = {};
  return execve(filename, argv, envp);
}

int execvpe(const char *filename, char *const argv[], char *const envp[]){
  char *resolved = ld_wrap_resolv(filename);
  int status = execve(resolved, argv, envp);
  free(resolved);
  return status;
}

int execvp(const char *filename, char *const argv[]){
  char *const envp[] = {};
  return execvpe(filename, argv, envp);
}
