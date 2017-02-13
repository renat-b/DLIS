#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"

int main()
{

    CDLISParser    parser;

    parser.Parse("D:\\src\\DLIS\\Dlis_examples\\Sample2.dlis");
    parser.Shutdown();

    return 0;
}