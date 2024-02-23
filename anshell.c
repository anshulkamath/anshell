#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// making some assumptions here because my brain doesn't wanna work that hard
#define MAX_CMD_LEN 120
#define MAX_ARG_LEN 20
#define MAX_PATH_LEN 4096
#define MAX_STR_LEN 256

/***********************
 *  GLOBAL VARIABLES   *
 ************************/
char HOME[MAX_PATH_LEN] = {0};
char PATH[MAX_PATH_LEN] = "/bin";

/***********************
 *  TYPE DEFINITIONS   *
 ************************/
typedef int (*cmd)(int, char **);
typedef struct built_in_s {
  char *name;
  cmd fn;
} built_in_t;

/***********************
 * FUNCTIONAL COMMANDS *
 ************************/

void initialize_environment();
int read_file(char *fname);
char *read_command(FILE *f);
int read_and_process_command(FILE *f);
int process_command(char *command);
int parse_command(char **dest, size_t len, char *command);
cmd get_command(char *str);
int find_and_exec_command_in_path(int argc, char **argv);
int print_error(char *err);

/***********************
 *  BUILT-IN COMMANDS  *
 ************************/

int pwd_cmd(int argc, char **argv);
int cd_cmd(int argc, char **argv);
int path_cmd(int argc, char **argv);
int exit_cmd(int argc, char **argv);

const built_in_t built_ins[] = {
    {.name = "pwd", .fn = pwd_cmd},
    {.name = "cd", .fn = cd_cmd},
    {.name = "path", .fn = path_cmd},
    {.name = "exit", .fn = exit_cmd},
    {.name = NULL, .fn = NULL},
};

int pwd_cmd(int argc, char **argv) {
  char *wd = NULL;
  size_t len;

  if ((wd = getcwd(NULL, len)) == NULL) {
    print_error(NULL);
  }

  printf("%s\n", wd);
  free(wd);

  return 0;
}

int cd_cmd(int argc, char **argv) {
  if (argc >= 3) {
    print_error("error: cd has too many arguments");
  }

  if (argc == 1) {
    chdir(HOME);
    return 0;
  }

  chdir(argv[1]);
  return 0;
}

int path_cmd(int argc, char **argv) {
  memset(PATH, 0, MAX_PATH_LEN);
  if (argc == 1) {
    return 0;
  }

  strcat(PATH, argv[1]);
  for (int i = 2; i < argc; i++) {
    strcat(PATH, ":");
    strcat(PATH, argv[i]);
  }

  return 0;
}

int exit_cmd(int argc, char **argv) {
  if (argc > 1) {
    print_error("an error occurred.");
    return 1;
  }

  exit(0);
}

/***********************
 *   IMPLEMENTATIONS   *
 ***********************/

void initialize_environment() {
  char *cwd = getcwd(NULL, 0);
  if (!strlen(cwd)) {
    print_error("unable to get home directory");
  }
  strcpy(HOME, cwd);
}

int read_file(char *fname) {
  FILE *f = fopen(fname, "r");

  if (!f) {
    print_error("an error occurred.");
    return 1;
  }

  char *command = NULL;
  size_t len;
  getline(&command, &len, f);
  while (!feof(f)) {
    printf("anshell> %s", command);
    process_command(command);
    command = NULL;
    getline(&command, &len, f);
  }

  if (fclose(f)) {
    print_error(NULL);
  }

  return 0;
}

int read_and_process_command(FILE *f) {
  char *command = NULL;
  size_t cap, len;
  printf("anshell> ");

  if (!(command = read_command(stdin))) {
    return 1;
  }

  return process_command(command);
}

char *read_command(FILE *f) {
  char *command_str = NULL;
  size_t cap;
  size_t len = getline(&command_str, &cap, f);

  if (feof(f)) {
    exit(0);
  } else if (len < 0) {
    print_error(NULL);
    return NULL;
  }

  return command_str;
}

int process_command(char *command) {
  int argc;
  char *argv[MAX_ARG_LEN];

  // copy the command, but get rid of the newline token
  char command_copy[MAX_CMD_LEN];
  memcpy(command_copy, command, MAX_CMD_LEN);
  strtok(command_copy, "\n");

  argc = parse_command(argv, MAX_ARG_LEN, command);

  if (!argc) {
    return print_error("bad format");
  }

  cmd fn = get_command(argv[0]);
  if (!fn) {
    if (!find_and_exec_command_in_path(argc, argv)) {
      return 1;
    }
    
    char err[MAX_STR_LEN] = "unknown command: ";
    strcat(err, argv[0]);
    return print_error(err);
  }

  return (*fn)(argc, argv);
}

int parse_command(char **dest, size_t len, char *command) {
  const char *sep = " \n";

  char *curr = strtok(command, sep);
  size_t i;

  for (i = 0; i < len; i++) {
    // note that we are using pointers to a broken string here
    dest[i] = curr;
    curr = strtok(NULL, sep);
    if (!curr) {
      break;
    }
  }

  return ++i;
}

cmd get_command(char *str) {
  char *cmd;
  for (int i = 0; built_ins[i].name; i++) {
    cmd = built_ins[i].name;
    if (!strcmp(cmd, str)) {
      return built_ins[i].fn;
    }
  }
  return NULL;
}

int find_and_exec_command_in_path(int argc, char **argv) {
  if (!strlen(PATH)) {
    return 1;
  }
  
  char *dir = strdup(PATH);
  char *curr = strtok(dir, ":");
  char bin_path[MAX_PATH_LEN] = {0};

  // null-terminate for execv
  argv[argc] = NULL;

  do {
    memset(bin_path, 0, MAX_PATH_LEN);
    strcat(bin_path, curr);
    strcat(bin_path, "/");
    strcat(bin_path, argv[0]);

    if (!access(bin_path, X_OK)) {
      break;
    }
  } while ((curr = strtok(NULL, ":")));

  if (curr == NULL) {
    free(dir);
    return 1;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    print_error("an error occurred");
    exit(1);
  } else if (!child_pid && execv(bin_path, argv)) {
    exit(EXIT_FAILURE);
  }

  int status;
  pid_t ret;
  if ((ret = wait(NULL))) {
    if (ret < 0) {
      exit(1);
    }

    fprintf(stdin, "process %d was terminated or killed\n", ret);
  }

  free(dir);
  return 0;
}

int print_error(char *err) {
  fprintf(stderr, "error occurred: %s\n", err ? err : strerror(errno));
  return 1;
}

/***********************
 *    MAIN FUNCTION    *
 ***********************/

int main(int argc, char **argv) {
  char *token;

  initialize_environment();

  if (argc > 2) {
    return print_error("an error occurred.");
  }

  while (1) {
    read_and_process_command(stdin);
  }

  return 1;
}
