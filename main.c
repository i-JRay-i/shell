#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUTLEN 4096
#define PATHLEN 4096
#define ARGNUM 256

typedef enum builtin_cmd {usrcmd = 0, cd, ex, echo, pwd, type} shCmd;

/* Reads user command from the standard input. */ 
void get_command(char *cmd_str, int maxlen) {
  int ch = 0;
  int len = 0;
  while (((ch = getchar()) != EOF && ch != '\n') && len < maxlen) {
    cmd_str[len] = ch;
    len++;
  }
  cmd_str[len] = '\0';
}

int run_command(char *cmd_path, char **cmd_args) {
  int status = 0;
  int ret;

  pid_t wpid;
  pid_t pid = fork();
  if (pid == 0) {
    if ((ret =execvp(cmd_path, cmd_args)) == -1)
      return -1;
    exit(ret);
  } else {
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 0;
}

/* Fills token with one character. */ 
void fill_token(char* token, char ch, int *token_idx, bool *token_in) {
  if (!*token_in)
    *token_in = true;
  token[*token_idx] = ch;
  *token_idx += 1;
}

/* Expands argument list with a new token. */
void add_token(char *token, char **args, int *token_num, int *token_len) {
  token[*token_len] = '\0';
  args[*token_num] = malloc(sizeof(char) * (*token_len));
  strcpy(args[*token_num], token);
  *token_num += 1;
  *token_len = 0;
}

/* Tokenizes the command line input. */
int tokenize(char *input_str, char **args, int maxlen) {
  int token_num = 0;
  int token_len = 0;
  bool token_in = false;
  bool single_quote_in = false;
  bool double_quote_in = false;
  bool esc_sec = false;
  char temp[1024];

  for (int input_idx = 0; input_idx < maxlen; input_idx++) {
    char ch = input_str[input_idx];
    if (ch == '\0') {
      if (token_len > 0)
        add_token(temp, args, &token_num, &token_len);
      break;
    }
    switch (ch) {
      case ' ': // space
        if (esc_sec) {
          fill_token(temp, ch, &token_len, &token_in);
          esc_sec = false;
          break;
        }
        if (token_in) {
          if (!single_quote_in && !double_quote_in) {
            add_token(temp, args, &token_num, &token_len);

            token_in = false;
            single_quote_in = false;
            double_quote_in = false;
          } else {
            fill_token(temp, ch, &token_len, &token_in);
          }
        }
        break;
      case '\\': // backslash
        if (single_quote_in) {
          fill_token(temp, ch, &token_len, &token_in);
        } else {
          if (double_quote_in) {
            if (!esc_sec) {
              esc_sec = true;
              fill_token(temp, ch, &token_len, &token_in);
            } else {
              esc_sec = false;
              break;
            }
          } else {
            if (!esc_sec) {
              esc_sec = true;
            } else {
              fill_token(temp, ch, &token_len, &token_in);
              esc_sec = false;
            }
          }
        }
        break;
      case '\'': // single quote
        if (esc_sec) {
          fill_token(temp, ch, &token_len, &token_in);
          esc_sec = false;
          break;
        }
        if (double_quote_in) { 
          fill_token(temp, ch, &token_len, &token_in);
        } else {
          if (!single_quote_in) {
            single_quote_in = true;
            token_in = true;
          } else {
            single_quote_in = false;
          }
        }
        break;
      case '"': // double quote
        if (esc_sec) {
          if (double_quote_in)
            token_len--;
          fill_token(temp, ch, &token_len, &token_in);
          esc_sec = false;
          break;
        }
        if (single_quote_in) {
          fill_token(temp, ch, &token_len, &token_in);
          break;
        }
        if (!double_quote_in) {
          double_quote_in = true;
          token_in = true;
        } else {
          double_quote_in = false;
        }
        break;
      default:
        fill_token(temp, ch, &token_len, &token_in);
        esc_sec = false;
        break;
    }
  }
  return token_num;
}

/* Control condition for evaluating user command */ 
shCmd eval_command(char *cmd) {
  if (!strcmp(cmd, "type")) {
    return type;
  } else if (!strcmp(cmd, "exit")) {
    return ex;
  } else if (!strcmp(cmd, "echo")) {
    return echo;
  } else if (!strcmp(cmd, "pwd")) {
    return pwd;
  } else if (!strcmp(cmd, "cd")) {
    return cd;
  } else {
    return usrcmd;
  }
}

/* Parses the pathnames in the PATH environment variable. */
int parse_path(char *path, char *cmd, char *cmd_path) {
  int cmd_found = 0;
  char full_cmd_path[PATHLEN];
  char *curr_path;

  full_cmd_path[0] = '\0';
  curr_path = strtok(path, ":");

  while (curr_path != NULL) {
    strcat(full_cmd_path, curr_path);
    strcat(full_cmd_path, "/");
    strcat(full_cmd_path, cmd);
    if ((cmd_found = access(full_cmd_path, F_OK)) == 0) {
      strcpy(cmd_path, full_cmd_path);
      break;
    }
    full_cmd_path[0] = '\0';
    curr_path = strtok(NULL, ":");
  }
  return cmd_found;
}

int main() {
  char user_input[INPUTLEN];          // User input buffer
  char cmd_path[PATHLEN] = " ";       // User command path
  char env_path[PATHLEN];             // PATH environment variable
  char curr_path[PATHLEN];            // Current working directory
  char home_path[PATHLEN];            // User's home path
  char *args[ARGNUM];                 // Number of arguments in the command

  for (int arg_idx = 0; arg_idx < ARGNUM; arg_idx++)
    args[arg_idx] = NULL;
  strcpy(home_path, getenv("HOME"));

  while(1) {
    // Prompt message
    printf("$ ");
    //fflush(stdout);

    // Wait for user input
    get_command(user_input, INPUTLEN);

    // Argument tokenizing
    int num_args = tokenize(user_input, args, INPUTLEN);
    shCmd cmd = eval_command(args[0]);

    // Shell logic
    switch (cmd) {
      case type: {
          if (args[1] == NULL) {
            break;
          }
          shCmd cmd_name = eval_command(args[1]);
          strcpy(env_path, getenv("PATH"));    // Environment path
          if (cmd_name > usrcmd) {
            printf("%s is a shell builtin\n", args[1]);
          } else {
            if ((parse_path(env_path, args[1], cmd_path)) != -1) {
              printf("%s is %s\n", args[1], cmd_path);
            } else {
              printf("%s: not found\n", args[1]);
            }
          }
        }
        break;
      case cd:
        if (!strcmp(args[1], "~"))
          chdir(home_path);
        else if (chdir(args[1]) != 0) {
          printf("%s: No such file or directory\n", args[1]);
        }
        break;
      case ex:
        if (args[1] != NULL) {
          int exit_code = atoi(args[1]);
          exit(exit_code);
        } else {
          exit(0);
        }
        break;
      case echo:
        for (int arg_idx = 1; arg_idx < num_args; arg_idx++) {
          printf("%s", args[arg_idx]);
          if (arg_idx != num_args - 1) {
            printf(" ");
          }
        }
        printf("\n");
        break;
      case pwd:
        if (getcwd(curr_path, PATHLEN) != NULL) {
          printf("%s\n", curr_path);
        }
        break;
      case usrcmd:
        strcpy(env_path, getenv("PATH"));
        if ((parse_path(env_path, args[0], cmd_path)) != -1) {
          int ret = run_command(args[0], args);
          if (ret == -1)
            printf("%s: not found\n", args[0]);
          break;
        } else if (access(args[0], F_OK) == 0) {
          int ret = run_command(args[0], args);
          if (ret == -1)
            printf("%s: not found\n", args[0]);
          break;
        } else {
          printf("%s: not found\n", args[0]);
          break;
        }
        break;
    }

    // Freeing the memory of previous tokens after the command is analyzed
    for (int arg_idx = 0; arg_idx < num_args; arg_idx++) {
      free(args[arg_idx]);
      args[arg_idx] = NULL;
    }
  }

  return 0;
}
