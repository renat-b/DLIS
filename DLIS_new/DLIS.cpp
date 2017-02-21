#include <stdio.h>
#include <tchar.h>
#include "DLISParser.h"


int main()
{

    g_global_log = new CFileBin;

    g_global_log->SetTestMode(true);

    if (g_global_log->OpenRead("../Dlis_examples/TestEWLDlis_1485963328769.bak.dat"))
    {
        CDLISParser  parser;

        parser.Parse("../Dlis_examples/TestEWLDlis_1485963328769.dlis");
        parser.Shutdown();

        g_global_log->Close();
    }

    delete g_global_log;

    system("pause");
    return 0;
}