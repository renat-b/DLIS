#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"

int main()
{
    CDLISParser    parser;

    parser.Parse("D:\\src\\DLIS task\\Dlis examples\\Sample2.dlis");
    //parser.Parse("D:\\src\\DLIS task\\new.dlis");
    parser.Shutdown();
    return 0;
}