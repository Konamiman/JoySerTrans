#ifndef __FILES_H
#define __FILES_H

#include "types.h"

byte CreateFile(char* fileName);
void CloseFile(byte fileHandle);
byte WriteToFile(byte* address, uint size);
int ReadFromFile(byte fileHandle, uint address);
byte OpenFile(char* fileName);
bool FileIsEmpty(byte fileHandle);

#endif