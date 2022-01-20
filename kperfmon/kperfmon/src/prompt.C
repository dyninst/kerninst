// prompt.C

#include "prompt.h"
#include "tkTools.h"
#include "common/h/String.h"
#include <unistd.h> // read()
#include <string.h>

extern Tcl_Interp *global_interp; // main.C

static pdstring command_so_far;
static void command_file_handler(ClientData cd, int /* mask */) {
   int fd = (int)cd;
   
   char stdin_buffer[1000];
   int count = read(fd, stdin_buffer, 999);
   bool error_or_eof = (count <= 0);
   if (error_or_eof) {
      // If we're a tty, then there's a real human typing at a real keyboard
      // who entered the eof character (^D I guess).  So I guess they really
      // want to quit.  If not a tty, but fd is fileno(stdin), then we stdin has
      // be rerouted to come in from a file or pipe; this file/pipe has ended so
      // there's not much anything more to come in.
      if (isatty(fd) || fd == fileno(stdin)) {
         cout << "kperfmon detected tty or stdin input EOF," << endl;
         cout << "so exiting kperfmon now." << endl;
         Tcl_DeleteFileHandler(fd);
         myTclEval(global_interp, "exit");
         return;
      }

      // OK: input is neither from a tty nor from stdin (redirected or not).
      // So in other words, input is from a file or pipe, which is not stdin.
      // So, although the file has nothing left to give, stdin is still open
      // to the possibility of the user typing stuff.  Let's leave that
      // possibility open, and not exit kperfmon entirely.
      cout << "kperfmon detected EOF for redirected input." << endl;
      cout << "Input can now take place from stdin console." << endl;
      
      Tcl_DeleteFileHandler(fd);
      Tcl_CreateFileHandler(fileno(stdin), TCL_READABLE, command_file_handler,
                            (ClientData)fileno(stdin));

      return;
   }

   stdin_buffer[count] = '\0';

   while (stdin_buffer[0] != '\0') {
      // there's something in the stdin_buffer to process
      char *semicolon_ptr = strchr(stdin_buffer, ';');
      char *eoln_ptr = strchr(stdin_buffer, '\n');
      if (semicolon_ptr == NULL && eoln_ptr == NULL) {
         // this doesn't end the command.  Just append to command_so_far and return.
         command_so_far += stdin_buffer;
         return;
      }

      // end of command.  Change end_of_command_ptr to \0, append to command_so_far,
      // shift remainder of stdin_buffer, and redo the while loop.
      char *end_of_command_ptr = (semicolon_ptr == NULL) ? eoln_ptr :
                                 (eoln_ptr == NULL) ? semicolon_ptr :
                                 (eoln_ptr < semicolon_ptr) ? eoln_ptr : semicolon_ptr;
      assert(end_of_command_ptr);
      *end_of_command_ptr = '\0';
      command_so_far += stdin_buffer; // this is what we'll execute

      char *leftover = end_of_command_ptr + 1;
      unsigned bytes_to_move = strlen(leftover) + 1; // include the \0 in the move
      memmove(stdin_buffer, leftover, bytes_to_move);

      // ready to evaluate "command_so_far".  Note that stdin_buffer may still have
      // some stuff left in it; hence the while loop we're in.

//        cout << "KernInst stdin: processing tcl command \""
//             << command_so_far << "\"" << endl;

      // turn off file handler during myTclEval to prevent reentry:
      Tcl_DeleteFileHandler(fd);
      myTclEval(global_interp, command_so_far, false);
         // false --> don't barf (exit(5)) on error...instead, print out error
         // and move on as if nothing happened.
      Tcl_CreateFileHandler(fd, TCL_READABLE, command_file_handler,
                            (ClientData)fd);

      // start the next command:
      cout << "kperfmon> " << flush;

      // despite SGI's documentation, there does not appear to be any clear() method
      // for strings:
      //command_so_far.clear();
      command_so_far = "";
   }

   // nothing left in the input buffer
}

int command_file_fd = -1;

void installTheTtyHandler(ClientData) {
   Tcl_CreateFileHandler(command_file_fd, TCL_READABLE, command_file_handler,
                         (ClientData)command_file_fd);
}

