#include    <stdio.h>
#include    <tchar.h>
#include    "DLISParser.h"

void  NotifyFrame(CDLISFrame *frame, void *params)
{
    return;
    static  int count = 0;

    float   *d;
    int      k = 0;

	int column = frame->CountColumns();
	if (column < 1)
		return;

    if (count < 100)
    {
        if (count == 0)
        {
            for (int i = 0; i < frame->CountColumns(); i++)
            {
                printf("%s", frame->GetColumnName(i));

                if (i != (frame->CountColumns() - 1))
                    printf("\t");
            }
            printf("\n");
        }

        for (int i = 0; i < frame->CountRows(); i++)
            for (int j = 0; j < frame->CountColumns(); j++)
            {
                if (j == 0)
                    printf("#%d\t", frame->GetNumber(i));

                d = frame->GetValueFloat(j, i, &k);
                double ret = *d;

                printf("value: %.5f  count: %d", ret, k);
                if (j != (frame->CountColumns() - 1))
                    printf("\t");
            }
        printf("\n");
    }
    count++;
}


void Usage()
{
    printf("Usage: \n"
           "-p \"c:\\example.dlis\"\n"
          );
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        Usage();
        return -1;
    }


    int   i = 1;
    char  dlis_path[MAX_PATH] = {0};

    while (i < argc)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            if ((i + 1) >= argc)
            {
                Usage();
                return -1;
            }

            i++;
            strcpy_s(dlis_path, argv[i]);
        }
        i++;        
    }
    
    if (dlis_path[0] == 0)
    {
        Usage();
        return -1;
    }


    bool r;
    CDLISParser  parser;

    parser.Initialize();
	parser.CallbackNotifyFrame(&NotifyFrame, 0);

    wchar_t buff[260] = { 0 };
    MultiByteToWideChar(CP_ACP, 0, dlis_path, (int)strlen(dlis_path), buff, _countof(buff)); 
    r = parser.Parse(buff);
    //printf("all frames: %d, bad frames: %d\n", parser.CountAllFrames(), parser.CountBadFrames());

    parser.Shutdown();

    return 0;
}