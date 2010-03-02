/*
  File: main.c
  Author: James Oakley
  Project: Katana (testing)
  Date: February 2010
  Description: Test program to demonstrate adding a variable to a program
*/

#include <stdio.h>
#include <unistd.h>

void printThings();

int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    usleep(100000);
  }
  return 0;
}
