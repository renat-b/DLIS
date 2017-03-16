#pragma once

#include "DlisCommon.h"


class CDLISFrame
{
private:
    DlisChannelInfo  *m_channels;
    int               m_frame_len;
    int               m_size_channels;
    int               m_count;
    char             *m_raw_data;
    int               m_raw_data_len;
    int               m_first_number;

public:
    CDLISFrame();
    ~CDLISFrame();

    void            Initialize(char *raw_data, int raw_data_len, DlisChannelInfo *channels, int size_channels, int first_number);
    //
    int             GetNumber(int column);
    char           *GetName(int column);
    double         *GetValueDouble(int column, int row, int *dimension);
    int            *GetValueInt(int column, int row, int *dimension);
    size_t          Count();

private:
    inline void     Big2LittelEndian(void *dst, int len);

    void           *GetValue(int column, int row, int *dimension);
};