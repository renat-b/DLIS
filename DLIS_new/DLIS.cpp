#include    <stdio.h>
#include    <tchar.h>
#include    "DLISParser.h"

void *  _memccpy(
    void * dest,
    const void * src,
    int c,
    unsigned count
    )
{
    while (count && (*((char *)(dest = (char *)dest + 1) - 1) =
        *((char *)(src = (char *)src + 1) - 1)) != (char)c)
        count--;

    return(count ? dest : NULL);
}

void  NotifyFrame(CDLISFrame *frame, void *params)
{
    static  int count = 0;

    float   *d;
    int      k = 0;
	double*  time;
	int      dimension;

	int column = frame->Columns();
	if (column < 1)
		return;

	for (int i = 0; i < frame->Rows(); i++)
	{
		dimension = 0; 
		time = frame->GetValueDouble(0, i, &dimension);

        if (!time)
        {
            return;
        }

        if (*time > 1499999999)
        {
            return;
        }

	}
    if (count < 100)
    {
        if (count == 0)
        {
            for (int i = 0; i < frame->Columns(); i++)
            {
                printf("%s", frame->GetName(i));

                if (i != (frame->Columns() - 1))
                    printf("\t");
            }
            printf("\n");
        }

        for (int i = 0; i < frame->Rows(); i++)
            for (int j = 0; j < frame->Columns(); j++)
            {
                if (j == 0)
                    printf("#%d\t", frame->GetNumber(i));

                d = frame->GetValueFloat(j, i, &k);
                double ret = *d;

                printf("value: %.5f  count: %d", ret, k);
                if (j != (frame->Columns() - 1))
                    printf("\t");
            }
        printf("\n");
    }
    count++;
}


int main()
{
    char dst[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    char src[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    
    _memccpy(dst, src, 100, sizeof(dst));

    g_global_log = new CFileBin;

    bool r;
    CDLISParser  parser;

    parser.Initialize();
	parser.CallbackNotifyFrame(&NotifyFrame);

    r = parser.Parse("G:\\Data\\08.16.2017\\Ewl_2017_08_09_18-31-01_(2)_Clb__.dlis");
    parser.Shutdown();

    return true;











    g_global_log->SetTestCompareFilesMode(false);
    g_global_log->SetPrintMode(false);

    if (g_global_log->IsCompareFilesMode())
        if (!g_global_log->OpenRead("../Dlis_examples/Sample2.dat"))
            return -1;
        



    parser.CallbackNotifyFrame(&NotifyFrame);
    parser.CallbackNotifyParams(NULL);

    LARGE_INTEGER start, end, elapsed, fraquency;

    QueryPerformanceFrequency(&fraquency);
    QueryPerformanceCounter(&start);
    

    parser.Initialize();
    r = parser.Parse("G:\\Data\\dr5\\EwlDlis_2017_05_27 01-53-22_TimeClb.dlis");

    parser.Initialize();
    r = parser.Parse("G:\\Data\\dr5\\EwlDlis_2017_05_27 01-53-52_(SNL_S).dlis");

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