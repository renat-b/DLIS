#include "DLISFrame.h"
#include "windows.h"

CDLISFrame::CDLISFrame() : m_channels(NULL), m_columns(0), m_frame_len(0), m_raw_data_len(0), m_raw_data(NULL)
{
}


CDLISFrame::~CDLISFrame()
{
}


char *CDLISFrame::GetName(size_t column)
{
    return NULL;
}


double *CDLISFrame::GetValueDouble(size_t column, size_t row, size_t *dimension)
{
    double  *ret;
    
    ret = (double *)GetValue(column, row, dimension);
    return ret;
}


int *CDLISFrame::GetValueInt(size_t column, size_t row, size_t *dimension)
{
    int  *ret;

    ret = (int *)GetValue(column, row, dimension);
    return ret;

}


size_t CDLISFrame::Count()
{
    return m_columns;
}


void CDLISFrame::Big2LittelEndian(void *data, size_t len)
{
    char *dst;
    char  tmp;

    dst = (char *)data;

    switch (len)
    {
        case 2:
            tmp    = dst[0];
            dst[0] = dst[1];
            dst[1] = tmp;
            break;

        case 4:
            tmp    = dst[0];
            dst[0] = dst[3];
            dst[3] = tmp;

            tmp    = dst[1];
            dst[1] = dst[2];
            dst[2] = tmp;
            break;

        case 8:
            tmp    = dst[0];
            dst[0] = dst[7];
            dst[7] = tmp;

            tmp    = dst[1];
            dst[1] = dst[6];
            dst[6] = tmp;

            tmp    = dst[2];
            dst[2] = dst[5];
            dst[5] = tmp;

            tmp    = dst[3];
            dst[3] = dst[4];
            dst[4] = tmp;
            break;

    }
}

void *CDLISFrame::GetValue(size_t column, size_t row, size_t *dimension)
{
    void  *ret;
    
    if (column >= m_columns)
        return NULL;

    if (row >= m_rows)
        return NULL;

    if (!dimension)
        return NULL;

    char     *data, *ptr;
    DlisChannelInfo  *channel;

    channel = &m_channels[column];

    data = m_raw_data + m_frame_len * row + channel->offsets;
    ptr  = data;
    for (int i = 0; i < channel->dimension; i++)
    {
        Big2LittelEndian(ptr, channel->element_size);
        ptr += channel->element_size;
    }

    *dimension = channel->dimension;

    ret = (void *)data;
    return ret;

}
