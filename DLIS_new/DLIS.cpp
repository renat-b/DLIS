#include    <stdio.h>
#include    <tchar.h>
#include    "DLISParser.h"


void  NotifyFrame(CDLISFrame *frame, void *params)
{
    static  int count = 0;

    int     k = 0;
    float  *d;

    d = frame->GetValueFloat(0, 0, &k);
    if (count < 100)
    {
        double ret = *d;
        //printf("%.4f\n", ret);
    }
    count++;
}


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

    parser.RegNotifyFrameFunc(&NotifyFrame);
    parser.RegNotifyParams(NULL);

    LARGE_INTEGER start, end, elapsed, fraquency;

    QueryPerformanceFrequency(&fraquency);
    QueryPerformanceCounter(&start);

    parser.Parse("../Dlis_examples/Sample2.dlis");

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