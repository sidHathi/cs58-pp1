#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#define STRING_LEN 100
#define MAX_FILES_IN_DIR 1024

static bool parse_args(const int argc, const char** argv, char** dirname);

int
main(const int argc, const char** argv)
{
    char* dirname;
    if (!parse_args(argc, argv, &dirname)) {
      return 1;
    }
    int tnPipe[2];

    if (pipe(tnPipe) < 0) {
        printf("pipe failed");
        return 1;
    }
    write(tnPipe[1], argv[0], strlen(argv[0]));
    close(tnPipe[1]);

    
    int pid = fork();
    if (pid == 0) {
        char readChars[STRING_LEN];
        while (read(tnPipe[0], readChars, STRING_LEN)) {
          if (readChars != NULL) {
            printf("Reading from pipe! argv[0]: %s\n", readChars);
          }
        }
        close(tnPipe[0]);
        printf("Nothing left to read!\n");
        exit(0);
    }
    sleep(1);
    kill(pid, 9);

    return 0;
}

bool
parse_args(const int argc, const char** argv, char** dirname)
{
    if (argc < 2) {
      return false;
    }
    return true;
}

void
get_photo_refs(char* dirName, int* numPhotos, char** photoNames)
{
    // should open the directory provided, iterate through the files, adding each to photoNames, and incrementing the numPhotos counter
}


// DEMO CODE FROM ASSIGNMENT PAGE:
// prompt the user with message, and save input at buffer
// (which should have space for at least len bytes)
int input_string(char *message, char *buffer, int len) {

  int rc = 0, fetched, lastchar;

  if (NULL == buffer)
    return -1;

  if (message)
    printf("%s: ", message);

  // get the string.  fgets takes in at most 1 character less than
  // the second parameter, in order to leave room for the terminating null.  
  // See the man page for fgets.
  fgets(buffer, len, stdin);
  
  fetched = strlen(buffer);


  // warn the user if we may have left extra chars
  if ( (fetched + 1) >= len) {
    fprintf(stderr, "warning: might have left extra chars on input\n");
    rc = -1;
  }

  // consume a trailing newline
  if (fetched) {
    lastchar = fetched - 1;
    if ('\n' == buffer[lastchar])
      buffer[lastchar] = '\0';
  }

  return rc;
}
