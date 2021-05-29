#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <signal.h>

//Linked List for input
struct userinput
{
  struct userinput *prev;
  char *input;
  struct userinput *next;
};

//Linked List for Processes
struct process
{
  struct process *prev;
  int pid;
  int *status;
  struct process *next;
};

//global so we can use it to set  foreground mode
bool fgmode = false;

//This is the CD command, takes a input and goes to that dir if valid
//fails like regular cd if multiple args
void intodir(struct userinput *folder)
{
  if (folder->next == NULL || strlen(folder->next->input) == 0)
  {
    char *varName = "HOME";
    char *home = getenv(varName);
    chdir(home);
  }

  else if (folder->next->next != NULL && strlen(folder->next->next->input) != 0)
  {
    printf("too many arguments");
    fflush(stdout);
    return;
  }

  //Doesn't error out if the folder doesn't exist
  else
  {
    folder = folder->next;
    int lastletter = strcspn(folder->input, "\n");
    if (strlen(folder->input) - 1 == lastletter)
    {
      folder->input[lastletter] = '\0';
    }

    chdir(folder->input);
  }

  char dirtemp[256];
  getcwd(dirtemp, sizeof(dirtemp));
  printf("%s\n", dirtemp);
  fflush(stdout);
}
//exit program, clean process list,clean input
void completeprocess(struct process *currentproc, struct userinput *currentinput)
{
  struct process *temp = calloc(1, sizeof(struct process));
  while (currentproc != NULL)
  {
    temp = currentproc->next;
    free(currentproc);
    currentproc = temp;
  }
  free(temp);

  //clean up input
  struct userinput *temp2 = calloc(1, sizeof(struct userinput));
  while (currentinput != NULL)
  {
    temp2 = currentinput->next;
    free(currentinput);
    currentinput = temp2;
  }
  free(temp2);
  exit(0);
}
//take a userinput node, and change the value of the $$ by reference
void dollarsignreplacer(struct userinput *dollaredcmd)
{

  //get pidid
  pid_t pid = getpid();

  char *line = NULL;
  size_t string;
  //read pid max
  FILE *fp = fopen("/proc/sys/kernel/pid_max", "r");
  getline(&line, &string, fp);
  int pidmax = strlen(line);

  char *pidstring = calloc(pidmax, sizeof(char));

  sprintf(pidstring, "%d", pid);

  //combine holds new string
  //temp holds partial strings for check
  char *combined = calloc(1024 * strlen(pidstring), sizeof(char));
  char *temp = calloc(2024, sizeof(char));
  int dex = 0;
  int length = strlen(dollaredcmd->input) - 1;

  //parse the string 2 letters at a time
  //starts with a dex(index) of 0
  for (int i = 0; i < length; i++)
  {
    memcpy(temp, dollaredcmd->input + dex, 2);
    char *first = strchr(temp, '$');
    if (first == NULL)
    {
      memcpy(combined + strlen(combined), temp, 1);
      dex = dex + 1;
      continue;
    }
    char *second = strchr(first + 1, '$');
    if (second == NULL)
    {
      memcpy(combined + strlen(combined), temp, 1);
      dex = dex + 1;
      continue;
    }

    if (strlen(second) + 1 != strlen(first))
    {
      memcpy(combined + strlen(combined), temp, 2);
    }

    else if (strlen(second) + 1 == strlen(first))
    {
      strcat(combined, pidstring);
    }
    dex = dex + 2;
  }

  dollaredcmd->input = combined;
}

//Add a Background process node, to link list
void addprocessnode(struct process *head, int spawnPID, int *status)
{
  struct process *new = calloc(1, sizeof(struct process));
  new->pid = spawnPID;
  new->status = status;

  //create head
  if (head->pid == NULL)
  {
    head->pid = spawnPID;
    head->status = status;
    return;
  }
  //

  //insert new "tail"
  else if (head->next == NULL)
  {
    head->next = new;
    new->prev = head;
    return;
  }

  //insert new "head"
  while (head->next != NULL)
  {
    head = head->next;
  }
  head->next = new;
  new->prev = head;
}
//Check for compeleted background processes. Delete when done
//Print a Message to inform User
void checkbackground(struct process *head)
{

  struct process *temp = calloc(1, sizeof(struct process));
  temp = head;
  while (temp != NULL && temp->pid != NULL)
  {
    if (waitpid(temp->pid, temp->status, WNOHANG) == 0)
    {
      temp = temp->next;
      continue;
    }
    //for node in the middle
    printf("Background Process %d is Done: with status %d\n", temp->pid, WEXITSTATUS(*temp->status));
    fflush(stdout);
    if (temp->next != NULL && temp->prev != NULL)
    {
      struct process *temp2 = calloc(1, sizeof(struct process));

      temp2->next = temp->next;
      temp2->prev = temp->prev;

      temp->next->prev = temp2->prev;
      temp->prev->next = temp2->next;
      free(temp);
      temp = temp2->next;

      free(temp2);
      continue;
    }
    //single node
    else if (temp->prev == NULL && temp->next == NULL)
    {
      head->pid = NULL;
      head->status = NULL;
      break;
    }

    //head node,changing seems to be more challenging then I thought
    //Doesn't seem to want to allow for address to be changed and ported out, even though passed by reference

    //Recreate head and free up memory;
    else if (temp->prev == NULL)
    {
      head->pid = head->next->pid;
      head->status = head->next->status;
      temp = head->next->next;
      free(head->next);
      head->next = temp;
      temp = head->next;
    }

    //tailnode

    else if (temp->next == NULL)
    {
      temp->prev->next = NULL;
      free(temp);
      break;
    }
  }
}

//Prints messages for forground processes.
//When Quit via Key-press or process finish
void setstatus(int *fgStatus, char *statusmsg)
{
  // This should help supresspress messages on subsquent calls between foreground calls
  if (*fgStatus == -1)
  {
    return;
  }
  if (WIFSIGNALED(*fgStatus))
  {
    sprintf(statusmsg, "Terminiated By Signal %d\n", WTERMSIG(*fgStatus));
    printf("%s", statusmsg);
    fflush(stdout);
    *fgStatus = -1;
  }
  else if (WIFEXITED(*fgStatus))
  {
    sprintf(statusmsg, "Exit Status: %d\n", WEXITSTATUS(*fgStatus));
    *fgStatus = -1;
  }
}

/* Our signal handler for SIGTSTP */
void setfg(int signo)
{
  if (fgmode == false)
  {
    fgmode = true;
    char *message = "Foreground Only Mode ON\n:";
    write(STDOUT_FILENO, message, 25);
  }

  else if (fgmode == true)
  {
    char *message2 = "Foreground Only Mode OFF\n:";
    fgmode = false;
    write(STDOUT_FILENO, message2, 26);
  }
}
// Read a input file via the commandline and the < char
void readfile(struct userinput *current)
{
  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  FILE *fp = fopen(current->input, "r");
  //return if NULL
  if (fp == NULL)
  {
    printf("Could Not  Read Text File");
    return;
  }

  while (nread = getline(&line, &len, fp) != -1)
  {
    struct userinput *insert = calloc(1, sizeof(struct userinput));
    //This refused to all any strcpy or sprintf, so making memory space
    //should clean up at program close
    char *temp = calloc(strlen(line), sizeof(char));
    sprintf(temp, "%s", line);
    //not sure how this will work for grading without removing
    int lastletter = strcspn(temp, "\n");
    if (strlen(temp) - 1 == lastletter)
    {
      temp[lastletter] = '\0';
    }
    insert->input = temp;
    insert->next = current->next;
    insert->prev = current;
    current->next = insert;

    current = current->next;
  }
}

int main()
{
  //Create needed globals
  int t = 1;
  const maxlength = 2048;
  const maxinputs = 516;
  int fgStatus = 0;
  char statusmsg[35];

  //empty process head
  struct process *processhead = calloc(1, sizeof(struct process));
  processhead->pid = NULL;

  // Install our main signal handler
  struct sigaction sigint = {0};
  sigint.sa_handler = SIG_IGN;
  sigfillset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sigint, NULL);

  //Install our foreground mount handler handler
  struct sigaction sigtst = {0};
  sigtst.sa_handler = setfg;
  sigfillset(&sigtst.sa_mask);
  sigtst.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &sigtst, NULL);
  //infinite loop, exit on "exit" only
  while (t == 1)
  {
    //set for input
    char *input[maxlength];
    memset(input, '\0', sizeof(char) * maxinputs + 1);

    setstatus(&fgStatus, statusmsg);

    int bgStatus = 0;
    //Check Process
    checkbackground(processhead);
    //get inputs
    printf(": ");

    fflush(stdout);

    fgets(input, maxlength, stdin);

    // cancel empty inputs
    if (strlen(input) == 0 || strcspn(input, "\n") == 0)
    {

      continue;
    }

    bool background = false;

    //set up head
    struct userinput *inputhead = calloc(1, sizeof(struct userinput));
    struct userinput *prev = calloc(1, sizeof(struct userinput));
    struct userinput *inputtail = calloc(1, sizeof(struct userinput));
    inputtail = NULL;
    char *token = strtok(input, " ");
    inputhead->input = token;

    //First dollar sign check
    if (strstr(inputhead->input, "$$") != NULL)
    {
      dollarsignreplacer(inputhead);
    }

    //second Node
    token = strtok(NULL, " ");
    if (token != NULL)
    {

      struct userinput *node = calloc(1, sizeof(struct userinput));
      node->input = token;
      node->prev = inputhead;
      inputhead->next = node;
      prev = node;
      inputtail = node;

      //second dollar sign check
      if (strstr(inputtail->input, "$$") != NULL)
      {
        dollarsignreplacer(inputtail);
        NULL;
      }
    }

    // add subsquent words as userinput structs

    while (token != NULL)
    {

      token = strtok(NULL, " ");
      if (token == NULL)
      {
        break;
      }
      struct userinput *node = calloc(1, sizeof(struct userinput));
      inputtail = node;
      node->input = token;
      prev->next = node;
      node->prev = prev;
      prev = node;
      // Repeated Dollar sign check
      if (strstr(inputtail->input, "$$") != NULL)
      {
        dollarsignreplacer(inputtail);
      }
    }

    //if statments multiple can be true
    if (inputhead->next == NULL)
    {

      inputhead->input[strlen(inputhead->input) - 1] = '\0';
    }

    if (strcspn(inputhead->input, "#") == 0)
    {
      continue;
    }

    if (inputtail != NULL)
    {

      inputtail->input[strlen(inputtail->input) - 1] = '\0';
    }

    if (inputtail != NULL && strstr(inputtail->input, "&"))
    {

      if (fgmode == false)
      {
        background = true;
      }
      if (fgmode == true)
      {
        background = false;
        inputtail->prev->next = NULL;
      }
    }
    struct userinput *current = calloc(1, sizeof(struct userinput));
    current = inputhead;
    //switch would be cleaner, but I think hard to implement without some changes
    if (strstr(inputhead->input, "cd"))
    {

      intodir(inputhead);
    }

    else if (strstr(inputhead->input, "status"))
    {

      printf("%s", statusmsg);

      fflush(stdout);
    }

    else if (strstr(inputhead->input, "exit"))
    {

      t = 2;
      completeprocess(processhead, inputhead);
    }

    // needs to be else if, because built in commands and this can't be executed together
    else if (background == false)
    {

      //Variables needed for process list, and exec
      char *newargv[maxinputs + 1];
      memset(newargv, '\0', sizeof(char) * maxinputs + 1);
      current = inputhead;
      int i = 0;
      int fd = -1;
      char *nulldir[2048];
      //create process list
      while (current != NULL)
      {

        //Parsed elsewhere
        if (strstr(current->input, "&") != NULL || strstr(current->input, "<") != NULL || strstr(current->input, ">") != NULL)
        {
          current = current->next;
          continue;
        }
        //Parse if previous: output to file
        if (current != inputhead && current->prev != NULL && strstr(current->prev->input, ">") != NULL)
        {
          sprintf(nulldir, "%s", current->input);
          fd = 0;
          current = current->next;
          continue;
        }
        //Parse if previous: Reads txt file into linked list
        if (current != inputhead && strstr(current->prev->input, "<") != NULL)
        {

          readfile(current);
          current = current->next;
          continue;
        }
        if (current->input != NULL)
        {

          newargv[i] = current->input;
          i++;
        }

        current = current->next;
      }

      // Fork a new process
      //get pid which is just a "int", but guarantees compatbily regardless of size.
      pid_t spawnPid = fork();

      // Fork a new process
      //   get pid which is just a "int", but guarantees compatbily regardless of size.
      switch (spawnPid)
      {
      case -1:
        perror("fork()\n");
        fflush(stderr);
        fflush(fd);
        exit(1);
      case 0:;

        //change for forks
        sigint.sa_handler = SIG_DFL;
        sigfillset(&sigint.sa_mask);
        sigint.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sigint, NULL);

        //stdout
        //Easiest to check for a toggle, which will tell us if output has been changed. Otherwise it is the default 1

        //If custom dir, then try to open fail and exit if can't
        if (fd == 0)
        {
          fd = open(nulldir, O_WRONLY | O_CREAT | O_TRUNC, 0640);
          if (fd == -1)
          {
            exit(1);
          }
          // Use dup2 to point FD 1, i.e., standard output to targetFD

          int result = dup2(fd, 1);
          if (result == -1)
          {
            exit(2);
          }
        }

        else
        {
          fd = 1;
        }

        execvp(newargv[0], newargv);
        // exec only returns if there is an error
        perror("");
        exit(1);

        fflush(stderr);
        fflush(fd);
      default:
          // In the parent process
          // Wait for child's termination
          //Block sigtstp
          ;
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGTSTP);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        spawnPid = waitpid(spawnPid, &fgStatus, 0);
        //cleanup after process
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        for (int i = 0; i < strlen(newargv); i++)
        {
          newargv[i] = NULL;
        }
        if (fd == -1)
        {
          fflush(stdout);
        }
      }
    }

    else if (background == true)
    {

      //Variables needed for Process List and Exec
      char *newargv[maxinputs + 1];
      memset(newargv, '\0', sizeof(char) * maxinputs + 1);
      char *nulldir[2048];
      sprintf(nulldir, "/dev/null");
      current = inputhead;
      int i = 0;
      int fd = -1;

      while (current != NULL)
      {

        //Parsed elsewhere
        if (strstr(current->input, "&") != NULL || strstr(current->input, "<") != NULL || strstr(current->input, ">") != NULL)
        {
          current = current->next;
          continue;
        }

        //Parse if previous: output to file
        if (current != inputhead && strstr(current->prev->input, ">") != NULL)
        {
          sprintf(nulldir, "%s", current->input);
          current = current->next;
          continue;
        }

        //Parse if previous: Reads txt file into linked list
        if (current != inputhead && strstr(current->prev->input, "<") != NULL)
        {
          readfile(current);
          current = current->next;
          fd = 0;
          continue;
        }

        if (current->input != NULL)
        {
          newargv[i] = current->input;
          i++;
        }
        current = current->next;
      }

      // Fork a new process
      //get pid which is just a "int", but guarantees compatbily regardless of size.
      pid_t spawnPid = fork();

      switch (spawnPid)
      {
      case -1:
        perror("fork()\n");
        fflush(stderr);
        fflush(fd);
        exit(1);
        break;
      case 0:;
        int fd = open(nulldir, O_WRONLY | O_CREAT | O_TRUNC, 0640);

        if (fd == -1)
        {
          exit(1);
        }
        // Use dup2 to point FD 1, i.e., standard output to targetFD
        int result = dup2(fd, 1);
        if (result == -1)
        {
          exit(2);
        }

        execvp(newargv[0], newargv);
        // exec only returns if there is an error
        perror("");
        fflush(stderr);
        fflush(fd);
        exit(1);
      default:
        printf("background pid is %d\n", spawnPid);
        //cleanup
        for (int i = 0; i < strlen(newargv); i++)
        {
          newargv[i] = NULL;
        }
        fflush(stdout);
        addprocessnode(processhead, spawnPid, &bgStatus);
      }
    }

    //clear out mermory
    struct userinput *temp = calloc(1, sizeof(struct userinput));
    current = inputhead;
    while (current != NULL)
    {
      temp = current->next;
      free(current);
      current = temp;
    }
    free(temp);
    free(current);
  }
}