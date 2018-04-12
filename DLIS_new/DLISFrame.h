#pragma once

#include   "DlisCommon.h"
#include   "MemoryBuffer.h"


class CDLISFrame
{
private:
    DlisChannelInfo  *m_channels;
    int               m_frame_len;
    int               m_channels_count;
    int               m_count;

    DlisValueObjName  m_obj_key;
    MemoryBuffer      m_buffer;
    MemoryBuffer      m_numbers;

public:
    CDLISFrame();
    ~CDLISFrame();
    bool            Initialize();
    void            Shutdown();

    bool            AddRawData(int number, char *raw_data, int raw_data_size);
    void            AddChannels(DlisValueObjName *object, DlisChannelInfo *channels, int channels_count, int frame_len);
    //
    int               GetNumber(int column);
    char             *GetColumnName(int column);
    DlisValueObjName *GetObject();

    float          *GetValueFloat(int column, int row, int *dimension);
    double         *GetValueDouble(int column, int row, int *dimension);
    int            *GetValueInt(int column, int row, int *dimension);

    int             CountColumns();
    int             CountRows();

private:
    inline void     Big2LittelEndian(void *dst, int len);

    void           *GetValue(int column, int row, int *dimension);
};