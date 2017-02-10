#include "Dlis.h"

using namespace Dlis;

int main(int argc, char *argv[])
{
    Reader dlis;

    dlis.open("D:\\src\\DLIS task\\Dlis examples\\Sample2.dlis"); 

    RI r = dlis.read();

	return 0;
}