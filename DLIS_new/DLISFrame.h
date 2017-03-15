#pragma once

#include "DlisCommon.h"


class CDLISFrame
{
private:
    DlisChannelInfo  *m_channels;
    size_t            m_frame_len;
    size_t            m_columns;
    size_t            m_rows;
    char             *m_raw_data;
    size_t            m_raw_data_len;

public:
    CDLISFrame();
    ~CDLISFrame();
    //
    char           *GetName(size_t index);
    double         *GetValueDouble(size_t column, size_t row, size_t *dimension);
    int            *GetValueInt(size_t column, size_t row, size_t *dimension);
    size_t          Count();

private:
    inline void     Big2LittelEndian(void *dst, size_t len);

    void           *GetValue(size_t column, size_t row, size_t *dimension);
};