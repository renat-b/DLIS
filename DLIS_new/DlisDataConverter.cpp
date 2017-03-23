#include "DlisDataConverter.h"


bool CDLISDataConverter::ReadRawData(void *dst, size_t len)
{

}


CDLISDataConverter::CDLISDataConverter()
{
    memset(&m_buffer, 0, sizeof(m_buffer));
}


CDLISDataConverter::~CDLISDataConverter()
{

}


bool CDLISDataConverter::GetFShort()
{
    uint8_t *ptr;

    ptr = &m_buffer[0];
    ReadRawData(ptr, 2);


    short  mantissa = ((*ptr << 8) | buf[1]) & 0XFFF0);
    short  exponent = ();

    return (double)M / (double)(1 << (15 - buf[1] & 0x0F));
}