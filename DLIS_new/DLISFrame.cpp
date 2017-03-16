#include "DLISFrame.h"
#include "windows.h"
#include "assert.h"


CDLISFrame::CDLISFrame() : m_channels(NULL), m_size_channels(0), m_frame_len(0), m_raw_data_len(0), m_raw_data(NULL), m_count(0), m_first_number(0)
{
}


CDLISFrame::~CDLISFrame()
{
}


void CDLISFrame::Initialize(char *raw_data, int raw_data_len, DlisChannelInfo *channels, int size_channels, int first_number)
{
    m_channels      = channels;
    m_size_channels = size_channels;
    m_raw_data      = raw_data;
    m_raw_data_len  = raw_data_len;
    m_first_number  = first_number;

    m_frame_len     = 0;
    for (int i = 0; i < size_channels; i++)
    {
        m_frame_len += channels->element_size;
    }

    if ((m_raw_data_len % m_frame_len) != 0)
        assert(false);

    m_count = m_raw_data_len / m_frame_len;
}


int CDLISFrame::GetNumber(int row)
{
    int number;

    number = m_first_number + row;
    return number;
}


char *CDLISFrame::GetName(int column)
{
    char *name;

    name = m_channels[column].obj_name->identifier;
    return name;
}


double *CDLISFrame::GetValueDouble(int column, int row, int *dimension)
{
    double  *ret;
    
    ret = (double *)GetValue(column, row, dimension);
    return ret;
}


int *CDLISFrame::GetValueInt(int column, int row, int *dimension)
{
    int  *ret;

    ret = (int *)GetValue(column, row, dimension);
    return ret;

}


size_t CDLISFrame::Count()
{
    return m_size_channels;
}


void CDLISFrame::Big2LittelEndian(void *data, int len)
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

void *CDLISFrame::GetValue(int column, int row, int *dimension)
{
    void     *ret;
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
