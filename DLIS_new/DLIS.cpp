#include    <stdio.h>
#include    <tchar.h>
#include    "DLISParser.h"


int main()
{
    g_global_log = new CFileBin;

    g_global_log->SetTestCompareFilesMode(false);
    g_global_log->SetPrintMode(false);

    if (g_global_log->IsCompareFilesMode())
        if (!g_global_log->OpenRead("../Dlis_examples/Sample2.dat"))
            return -1;
        

    CDLISParser  parser;

    parser.Initialize();

    LARGE_INTEGER start, end, elapsed, fraquency;

    QueryPerformanceFrequency(&fraquency);
    QueryPerformanceCounter(&start);

    parser.Parse("../Dlis_examples/TestEWLDlis_1485963328769.dlis");

    QueryPerformanceCounter(&end);
    elapsed.QuadPart = (LONGLONG)(double(end.QuadPart - start.QuadPart) / fraquency.QuadPart * 1000);
    
    printf("time: %I64u ms\n", elapsed.QuadPart);

    if (g_global_log->IsCompareFilesMode())
        g_global_log->Close();

    delete g_global_log;

    system("pause");

    parser.Shutdown();
    return 0;
}