#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"

int main()
{
    g_global_log = new CFileBin;
    if (g_global_log->OpenRead("../Dlis_examples/out.dat"))
    {
        CDLISParser  parser;

        parser.Parse("../Dlis_examples/Sample2.dlis");
        parser.Shutdown();

        g_global_log->Close();
    }

    delete g_global_log;
    return 0;
}