#include "Dlis.h"

using namespace Dlis;

int main(int argc, char *argv[])
{
    Reader  dlis;
    
    if (g_global_log.OpenWrite("../Dlis_examples/out.dat"))
    {
        dlis.open("../Dlis_examples/Sample2.dlis"); 

        RI r = dlis.read();
    
        g_global_log.Close();
    }

	return 0;
}