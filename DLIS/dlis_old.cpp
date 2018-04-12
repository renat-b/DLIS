#include "Dlis.h"

using namespace Dlis;

int main(int argc, char *argv[])
{
    Reader  dlis;
    
    //if (g_global_log.OpenWrite("../Dlis_examples/out_TestEWLDlis_1485963328769.dat"))
    //{
        dlis.open("G:\\Data\\08.18.2017\\DLIS_bad\\08_16_2017_STATION\\data\\Ewl_2017_08_16_16-10-53_(1)_Clb__.dlis"); 

        RI r = dlis.read();
    
    //    g_global_log.Close();
    //}

	return 0;
}