#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <glob.h>
#include <sys/wait.h>

#define STRING_LEN 100
#define MAX_FILES_IN_DIR 1024

static bool parse_args(const int argc, const char** argv, char** dirnames);
static void get_photo_refs(char** dirNames, int numDirs, int* numPhotos, char** photoPaths);
static void process_thumbnails(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe);
static void pipeline_images(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe)
static char* insert_path_suffix(char* originalPath, char* suffix);
static void gen_thumbnail(char* imagePath, char* destPath);
static void display_image(char* imagePath);

int
main(const int argc, const char** argv)
{
    char* dirnames[MAX_FILES_IN_DIR];
    if (!parse_args(argc, argv, dirnames)) {
      return 1;
    }
    char* photoPaths[MAX_FILES_IN_DIR];
    int numPhotos = 0;
    get_photo_refs(dirnames, argc - 1, &numPhotos, photoPaths);
    for (int i = 0; i < argc - 1; i ++ ) {
      free(dirnames[i]);
    }

    int tnPipe[2];
    int sizePipe[2];

    if (pipe(tnPipe) < 0) {
        printf("pipe failed");
        return 1;
    }
    if (pipe(sizePipe) < 0) {
      printf("pipe failed");
      return 1;
    }

    process_thumbnails(numPhotos, photoPaths, tnPipe, sizePipe);
    pipeline_images(numPhotos, photoPaths, tnPipe, sizePipe);

    int size0 = strlen(argv[0]);
    int size1 = strlen(argv[1]);
    write(sizePipe[1], &size0, sizeof(int));
    write(tnPipe[1], argv[0], size0);
    write(sizePipe[1], &size1, sizeof(int));
    write(tnPipe[1], argv[1], size1);
    close(tnPipe[1]);
    close(sizePipe[1]);

    int pid = fork();
    if (pid == 0) {
        int readSize;
        while (read(sizePipe[0], &readSize, sizeof(int))) {
          printf("read integer val: %d\n", readSize);
          char readChars[STRING_LEN];
          if (readSize > 0) {
            read(tnPipe[0], readChars, readSize);
            printf("read piped chars: %s\n", readChars);
            char* modStr = insert_path_suffix(readChars, "tn");
            printf("modded piped chars: %s\n", modStr);
            free(modStr);
            readSize = 0;
          }
        }
        close(tnPipe[0]);
        close(sizePipe[0]);
        printf("Nothing left to read!\n");
        exit(0);
    }
    sleep(1);
    kill(pid, 9);

    return 0;
}

bool
parse_args(const int argc, const char** argv, char** dirnames)
{
    if (argc < 2) {
      return false;
    }
    for ( int i = 1; i < argc; i ++ ) {
      dirnames[i-1] = malloc(STRING_LEN*sizeof(char));
      strcpy(dirnames[i-1], argv[i]);
    }
    return true;
}

void
get_photo_refs(char** dirNames, int numDirs, int* numPhotos, char** photoPaths)
{
    // should open the directory provided, iterate through the files, adding each to photoNames, and incrementing the numPhotos counter
    int photosAdded = 0;
    for ( int d = 0; d < numDirs; d ++ ) {
      char* dirName = dirNames[d];
      glob_t glob_result;
      glob(dirName, GLOB_TILDE, NULL, &glob_result);

      *numPhotos += glob_result.gl_pathc;
      for ( int i = 0; i < glob_result.gl_pathc; i ++ ) {
        photoPaths[photosAdded] = glob_result.gl_pathv[i];
        printf("photo found with path %s\n", photoPaths[photosAdded]);
        photosAdded ++;
      }

      globfree(&glob_result);
    }
    printf("%d photos found for regex\n", *numPhotos);
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

void
process_thumbnails(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe)
{
  int pid = fork();
  if (pid == 0) {
    for ( int i = 0; i < numPhotos; i ++ ) {
      int cpid = 0;
      cpid = fork();
      if (cpid == 0) {
        char* photoName = photoNames[i];
        char* photoTnName = insert_path_suffix(photoName, "tn");
        gen_thumbnail(photoName, photoTnName);
        exit(0);
      }
      int exitStatus;
      waitpid(cpid, &exitStatus, 0);

      int nameSize = strlen(photoNames[i]);
      write(sizePipe[1], &nameSize, sizeof(int));
      write(tnPipe[1], photoNames[i], nameSize);
    }
    exit(0);
  }
}

void
pipeline_images(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe)
{
  int pid = fork();
  if (pid == 0) {
    int thumbnailsCompleted = 0;
    int currPhotoIndex = 0;
    int readSize;
    while (read(sizePipe[0], &readSize, sizeof(int))) {
      char readChars[STRING_LEN];
      if (readSize > 0) {
        read(tnPipe[0], readChars, readSize);
        // handle the new string here
        // this includes -> figuring out the file it corresponds to
        // displaying the thumbnail to the use in a forked process
        char* photoTnName = insert_path_suffix(photoName, "tn");
        display_image(photoTnName);
        // asking the user if they want to flip the thumbnail or not
        // creating the new processed image
        // saving it
      }
    }

    exit(0);
  }
}

void
display_image(char* imagePath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "/bin/display";
    char* args[] = { "display", imagePath };
    execv(programPath, args);
    exit(0);
  }
}

void rotate_image(char* imagePath, char* destPath, int degrees)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "bin/convert";
    char degreeString[STRING_LEN];
    sprintf(degreeString, "%d", degrees);
    char* args[] = { "convert", "-rotate", degreeString, imagePath, destPath};
    execv(programPath, args);
    exit(0);
  }
}

void
gen_thumbnail(char* imagePath, char* destPath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "bin/convert";
    char* args[] = { "convert", "-resize", "10%", imagePath, destPath };
    execv(programPath, args);
    exit(0);
  }
  int exitStatus;
  waitpid(cpid, &exitStatus, 0);
}

void
gen_final_image(char* srcImagePath, char* destPath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "bin/convert";
    char* args[] = { "convert", "-resize", "25%", srcImagePath, destPath };
    execv(programPath, args);
    exit(0);
  }
}

char*
insert_path_suffix(char* originalPath, char* suffix)
{
  int stringSize = strlen(originalPath) + strlen(suffix) + 1;
  int extStart = strlen(originalPath) - 1;
  int pathSize = strlen(originalPath);
  for ( int i = 0; i < pathSize; i ++ ) {
    char c = originalPath[i];
    if (c == '.') {
      extStart = i;
    }
  }

  char prefix[STRING_LEN];
  char ext[STRING_LEN]; 
  strncpy(prefix, originalPath, extStart);
  prefix[extStart] = '\0';
  strncpy(ext, originalPath + extStart + 1, pathSize - (extStart + 1));
  ext[pathSize - (extStart + 1)] = '\0';
  char* newPath = malloc((stringSize + 1) * sizeof(char));
  sprintf(newPath, "%s_%s.%s", prefix, suffix, ext);
  return newPath;
}
