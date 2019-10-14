# Small Shell

### Description

This program is a shell application, with functionality similar to a simplified bash shell.  It was created for an assignment in my operating systems course.  It supports several built-in commands and additional commands from the linux environment.  When receiving a command, the program forks off a new process which executes the command.  The '&' operator can optionally be used to execute the command in the background.


### Dependencies

Linux environment


### Instructions

To compile and then run

```
gcc smallsh.c -o smallsh
./smallsh
```

To run a command, use the following syntax
```
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
```

The following 3 commands are built in to the shell

```
exit
```
This exits the program

```
cd
```
This changes the directory

```
status
```
This prints the exit status or terminating signal of the last foreground process

Any other commands which exist in the environment are supported by calls to the environment