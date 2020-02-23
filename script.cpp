#include "hwsetup.h"
#include <arduino.h>
#include "global_variables.h"
#include "burn_control.h"
#include "script.h"
#ifdef MPIEC_ENABLE_SCRIPT

#include <avil.h>


avil interpreter;
char cmd[30];

void setupSerialShell() {
  if(!interpreter.init()){
       Sys::userOutput(F("init error\n\r"));
       while(1);
   }
   cmd[0]='\0';
}

void handleSerialShellTask() {
   Sys::userOutput(F("\n\r>"));
   //get a user input
   if(Sys::userInput(cmd, 30)){
       Sys::userOutput(F("\n\r"));
       interpreter.run(cmd);
   }
   else{
       Sys::userOutput(F("bad input!\n"));
   }  
}


#endif
