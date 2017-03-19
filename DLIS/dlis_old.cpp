#include "Dlis.h"

using namespace Dlis;

int main(int argc, char *argv[])
{
    Reader  dlis;
    
    //if (g_global_log.OpenWrite("../Dlis_examples/out_TestEWLDlis_1485963328769.dat"))
    //{
        dlis.open("../Dlis_examples/TestEWLDlis_1485963328769.dlis"); 

        RI r = dlis.read();
    
    //    g_global_log.Close();
    //}

	return 0;
}