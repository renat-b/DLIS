#include "DlisDataConverter.h"


CDLISDataConverter::CDLISDataConverter()
{

}


CDLISDataConverter::~CDLISDataConverter()
{

}


double CDLISDataConverter::DLIS_FSHORT(unsigned char *buf, int &nbyte)
{
    nbyte = 2;
    short M = ((buf[0] << 8) | buf[1]) & 0XFFF0;
    return (double)M / (double)(1 << (15 - buf[1] & 0x0F));
}