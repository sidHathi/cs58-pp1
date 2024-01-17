/* 
 * album.c - CS58 Programming Project 1
 * 
 * The 'crawler' module defines an executable program that 
 * concurently converts input images into thumbnails,
 * and provides a command line UI to the user to determine
 * what further edits to make. After it executes, it leaves
 * behind an index.html file with links to the results
 * 
 * Takes in the names of the images as parameters
 *
 * Siddharth Hathi, January 2024
 */

#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <glob.h>
#include <sys/wait.h>
#include <errno.h>

/**************** file-local global variables ****************/
#define STRING_LEN 100
#define MAX_FILES_IN_DIR 1024

/**************** global types ****************/
typedef struct EditedImage {
  char* originalImagePath;
  char* thumbnailPath;
  char*  finalImagePath;
  char* caption;
} edited_image_t;

/**************** function prototypes ****************/
static bool parse_args(const int argc, const char** argv, char** dirnames);
static void get_photo_refs(char** dirNames, int numDirs, int* numPhotos, char** photoPaths);
static void process_thumbnails(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe);
static void pipeline_images(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe);
static char* insert_path_suffix(char* originalPath, char* suffix);
static void gen_thumbnail(char* imagePath, char* destPath);
static void gen_final_image(char* srcImagePath, char* destPath);
static int display_image(char* imagePath);
static void rotate_image(char* imagePath, char* destPath, int degrees);
static edited_image_t* edited_image_new(char* imagePath, char* tnPath, char* finalImagePath, char* caption);
static void edited_image_free(edited_image_t* ei);
static void gen_html(edited_image_t** editedImages, int numImages);

/* ***************** main ********************** */
int
main(const int argc, const char** argv)
{
    char* dirnames[MAX_FILES_IN_DIR];
    if (!parse_args(argc, argv, dirnames)) {
      return 1;
    }
    char** photoPaths = malloc(sizeof(char*)*MAX_FILES_IN_DIR);
    int numPhotos = 0;
    get_photo_refs(dirnames, argc - 1, &numPhotos, photoPaths);

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

    for (int i = 0; i < argc - 1; i ++ ) {
      free(dirnames[i]);
    }
    free(photoPaths);

    return 0;
}

/**************** parse_args ****************/
/* Parses command line arguments and updates dependent
 * variables.
 *
 * Caller provides:
 *   copy of argc, argv provided to main()
 *   *pointers* to seedURL, pageDirectory, and maxDepth
 *   variables that should be updated based on arguments
 * We guarantee:
 *   Program exits if argument parsing unnsuccessful -
 *   in this case function does not return and error message
 *   written to stderr
 *   Otherwise, seedURL, pageDirectory, and maxDepth will
 *   be updated based on argc, argv contents.
 */
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

/**************** get_photo_refs ****************/
/* Gets all the filepaths in the current directory matching
 * one of a list of regex expressions
 *
 * Caller provides:
 *   list of regex filenames
 *   the number of files
 *   an empty integer pointer
 *   an empty array of strings
 * We guarantee:
 *   the integer pointer will be populated with the number of matching files
 *   the array will be populated with the names of all match files
 */
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
        // FREE LATER
        photoPaths[photosAdded] = malloc(sizeof(char)*strlen(glob_result.gl_pathv[i]));
        strcpy(photoPaths[photosAdded], glob_result.gl_pathv[i]);
        photosAdded ++;
      }

      globfree(&glob_result);
    }
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

/**************** process_thumbnails ****************/
/* Forks a process to create each thumbnail and pipes
 * the names of the completed thumbnails through provided
 * pipe
 *
 * Caller provides:
 *   number of photos
 *   array of photo names
 *   pipe through which to send the thumbnails
 *   pipe through which to send the size of each thumbnail filename string
 * We guarantee:
 *   the forked process will execute the convert shell program for each of
 *   the photos, and the results will be piped through the input pipes;
 *   we also guarantee that the function will wait until the forked process
 *   terminates
 */
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
    close(tnPipe[1]);
    close(sizePipe[1]);
    exit(0);
  }
  int exitStatus;
  waitpid(pid, &exitStatus, 0);
}

/**************** pipeline_images ****************/
/* Forks a process to get user input for each image and
 * create the final processed image depending on that input
 *
 * Caller provides:
 *   number of photos
 *   array of photo names
 *   pipe through which to read the name of completed thumbnails
 *   pipe through which to read the size of each thumbnail filename string
 * We guarantee:
 *   the forked process will ask for input and modify an image only
 *   after the thumbnail pipeline has converted it first; the function
 *   will only terminate after the forked process has exited
 */
void
pipeline_images(int numPhotos, char** photoNames, int* tnPipe, int* sizePipe)
{
  int pid = fork();
  if (pid == 0) {
    int currPhotoIndex = 0;
    int readSize;
    edited_image_t** editedImages = malloc(numPhotos*sizeof(edited_image_t*));
    while (currPhotoIndex < numPhotos && read(sizePipe[0], &readSize, sizeof(int))) {
      char readChars[STRING_LEN];
      if (readSize > 0) {
        read(tnPipe[0], readChars, readSize);
        char* currPhotoName = photoNames[currPhotoIndex];
        // handle the new string here
        // this includes -> figuring out the file it corresponds to
        // displaying the thumbnail to the use in a forked process
        char* photoTnName = insert_path_suffix(currPhotoName, "tn");
        int dispPid = display_image(photoTnName);

        // asking the user if they want to flip the thumbnail or not
        bool rotated = false;
        char resp[STRING_LEN];
        char* rotationPath = insert_path_suffix(currPhotoName, "rot");
        input_string("Would you like to flip the image? (y/n)", resp, 4);
        if (resp[0] == 'y' || resp[0] == 'Y') {
          rotated = true;
          char dirResp[STRING_LEN];
          input_string("Type r for clockwise rotation and l for counterclockwise? (r/l)", dirResp, 4);
          int rotation = dirResp[0] == 'r' ? 90 : -90;
          // need to rotate both the current image and the thumbnail and store somewhere that the image was rotated
          rotate_image(currPhotoName, rotationPath, rotation);
          rotate_image(photoTnName, photoTnName, rotation);
        }

        char caption[STRING_LEN];
        input_string("Write a caption for the image", caption, STRING_LEN);

        // generate the final 25% image:
        char* finalImagePath = insert_path_suffix(currPhotoName, "final");
        if (rotated) {
          gen_final_image(rotationPath, finalImagePath);
        }
        edited_image_t* ei = edited_image_new(currPhotoName, photoTnName, finalImagePath, caption);
        editedImages[currPhotoIndex] = ei;

        free(photoTnName);
        free(rotationPath);
        free(finalImagePath);
        kill(dispPid, 9);
        currPhotoIndex ++;
      }
    }

    close(tnPipe[0]);
    close(sizePipe[0]);
    gen_html(editedImages, numPhotos);
    for ( int i = 0; i < numPhotos; i ++ ) {
      if (editedImages[i] != NULL) {
        edited_image_free(editedImages[i]);
        editedImages[i] = NULL;
      }
    }
    exit(0);
  }
  int exitStatus;
  waitpid(pid, &exitStatus, 0);
}

/**************** display_image ****************/
/* Forks and displays the specified image
 *
 * Caller provides:
 *   path of the image
 * Function returns:
 *   pid of display process
 */
int
display_image(char* imagePath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "display";
    char* args[] = { "display", imagePath, NULL };
    if (execvp(programPath, args) == -1) {
      perror("Error executing command");
      exit(-1);
    }
    exit(0);
  }
  return pid;
}

/**************** rotate_image ****************/
/* Forks and rotates the specified image
 *
 * Caller provides:
 *   path of the image
 *   path of rotated image
 *   degrees of rotation
 */
void
rotate_image(char* imagePath, char* destPath, int degrees)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "convert";
    char degreeString[STRING_LEN];
    char* args[] = { "convert", "-rotate", degreeString, imagePath, destPath, NULL };
    if (execvp(programPath, args) == -1) {
      perror("Error executing command");
      exit(-1);
    }
    exit(0);
  }
  int exitStatus;
  waitpid(pid, &exitStatus, 0);
  // int return_status = WEXITSTATUS(exitStatus);
}

/**************** rotate_image ****************/
/* Forks and compresses the specified image into a thumbnail
 *
 * Caller provides:
 *   path of the image
 *   path of the result
 */
void
gen_thumbnail(char* imagePath, char* destPath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "convert";
    char* args[] = { "convert", "-resize", "10%", imagePath, destPath, NULL };
    if (execvp(programPath, args) == -1) {
      perror("Error executing command");
      exit(-1);
    }
    exit(0);
  }
  int exitStatus;
  waitpid(pid, &exitStatus, 0);
}

/**************** gen_final_image ****************/
/* Forks and compresses the specified image into a 25% copy
 *
 * Caller provides:
 *   path of the image
 *   path of the result
 */
void
gen_final_image(char* srcImagePath, char* destPath)
{
  int pid = fork();
  if (pid == 0) {
    char* programPath = "convert";
    char* args[] = { "convert", "-resize", "25%", srcImagePath, destPath, NULL };
    if (execvp(programPath, args) == -1) {
      perror("Error executing command");
      exit(-1);
    }
    exit(0);
  }

  int exitStatus;
  waitpid(pid, &exitStatus, 0);
}

/**************** insert_path_suffix ****************/
/* Inserts a suffix into a filename. For filename sample.txt
 * and suffix o, the output would be sample_o.txt
 *
 * Caller provides:
 *   original filename
 *   suffix to be inserted
 * Returns:
 *  original filename with suffix inserted
 */
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

/**************** edited_image_new ****************/
/* Allocates and creates an edited image struct
 */
edited_image_t*
edited_image_new(char* imagePath, char* tnPath, char* finalImagePath, char* caption)
{
  edited_image_t* ei = malloc(sizeof(edited_image_t));
  ei->originalImagePath = malloc(strlen(imagePath)*sizeof(char));
  strcpy(ei->originalImagePath, imagePath);
  ei->thumbnailPath = malloc(strlen(tnPath)*sizeof(char));
  strcpy(ei->thumbnailPath, tnPath);
  ei->finalImagePath = malloc(strlen(finalImagePath)*sizeof(char));
  strcpy(ei->finalImagePath, finalImagePath);
  ei->caption = malloc(strlen(caption)*sizeof(char));
  strcpy(ei->caption, caption);
  return ei;
}

/**************** edited_image_free ****************/
/* Frees edited image struct
 */
void
edited_image_free(edited_image_t* ei)
{
  free(ei->originalImagePath);
  free(ei->thumbnailPath);
  free(ei->finalImagePath);
  free(ei->caption);
  free(ei);
}

/**************** gen_html ****************/
/* Generates an html file for a list of edited images
 *
 * Caller provides:
 *   listed of edited_image structs
 *   length of the list
 */
void
gen_html(edited_image_t** editedImages, int numImages)
{
  // needs to take each of the thumbnails captions, and final images as input
  // needs to overwrite and clear the index.html file
  // needs to add the necessary header contents (and closing tags at the end)
  // needs to loop over each of the images and create an img and a obj linking to the final result
  FILE* htmlFile = fopen("index.html", "w");
  if (htmlFile == NULL) {
    printf("error opening file");
    return;
  }

  fprintf(htmlFile, "<html>\n<body>\n");
  for ( int i = 0; i < numImages; i ++ ) {
    edited_image_t* ei = editedImages[i];
    fprintf(htmlFile, "<a href='%s'><img src='%s' /><p>%s</p></a>\n", ei->finalImagePath, ei->thumbnailPath, ei->caption);
  }
  fprintf(htmlFile, "</body>\n</html>");

  fclose(htmlFile);
}
