#include    <stdio.h>
#include    <tchar.h>
#include    "DLISParser.h"


int main()
{
    g_global_log = new CFileBin;

    g_global_log->SetTestCompareFilesMode(false);
    g_global_log->SetPrintMode(true);

    if (g_global_log->IsCompareFilesMode())
        if (!g_global_log->OpenRead("../Dlis_examples/Sample2.dat"))
            return -1;
        

    CDLISParser  parser;

    parser.Parse("../Dlis_examples/Sample2.dlis");
    parser.Shutdown();

    if (g_global_log->IsCompareFilesMode())
        g_global_log->Close();

    delete g_global_log;

    system("pause");
    return 0;
}