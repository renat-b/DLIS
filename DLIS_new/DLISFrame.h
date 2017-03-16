#pragma once

#include   "DlisCommon.h"
#include   "MemoryBuffer.h"


class CDLISFrame
{
private:
    DlisChannelInfo  *m_channels;
    int               m_frame_len;
    int               m_size_channels;
    int               m_count;
    int               m_first_number;

    MemoryBuffer      m_buffer;
    MemoryBuffer      m_numbers;

public:
    CDLISFrame();
    ~CDLISFrame();
    bool            Initialize();
    void            Shutdown();

    bool            AddRawData(int number, char *raw_data, int raw_data_size);
    void            AddChannels(DlisChannelInfo *channels, int size_channels, int frame_len);
    //
    int             GetNumber(int column);
    char           *GetName(int column);

    float          *GetValueFloat(int column, int row, int *dimension);
    double         *GetValueDouble(int column, int row, int *dimension);
    int            *GetValueInt(int column, int row, int *dimension);

    int             Count();

private:
    inline void     Big2LittelEndian(void *dst, int len);

    void           *GetValue(int column, int row, int *dimension);
};