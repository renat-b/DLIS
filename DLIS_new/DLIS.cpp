#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"

int main()
{

    CDLISParser    parser;

    parser.Parse("../Dlis_examples/Sample2.dlis");
    parser.Shutdown();

    return 0;
}