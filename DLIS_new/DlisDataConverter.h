#pragma once


class CDLISDataConverter
{
public:
    CDLISDataConverter();
    ~CDLISDataConverter();

    double DLIS_FSHORT(unsigned char *buf, int& nbyte);
};