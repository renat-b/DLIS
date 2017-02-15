#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"

int main()
{
    if (g_global_log.OpenRead("../Dlis_examples/out.dat"))
    {
        CDLISParser  parser;

        parser.Parse("../Dlis_examples/Sample2.dlis");
        parser.Shutdown();

        g_global_log.Close();
    }
    return 0;
}