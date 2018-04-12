#include "StdAfx.h"
#include "DLISFrame.h"
#include "windows.h"
#include "assert.h"


CDLISFrame::CDLISFrame() : m_channels(NULL), m_channels_count(0), m_frame_len(0), m_count(0)
{
    memset(&m_buffer, 0, sizeof(m_buffer));
    memset(&m_numbers, 0, sizeof(m_numbers));
}


CDLISFrame::~CDLISFrame()
{
}


bool CDLISFrame::Initialize()
{
    m_buffer.size  = 0;
    m_numbers.size = 0;

    return true;
}


void CDLISFrame::Shutdown()
{
    if (m_buffer.data)
        delete [] m_buffer.data;

    if (m_numbers.data)
        delete[] m_numbers.data;

    memset(&m_buffer, 0, sizeof(m_buffer));
    memset(&m_numbers, 0, sizeof(m_numbers));
}


bool CDLISFrame::AddRawData(int number, char *raw_data, int raw_data_size)
{
    // добавим данные
    if (!m_buffer.Resize(m_buffer.size + raw_data_size))
        return false;
    
    memcpy(m_buffer.data + m_buffer.size, raw_data, raw_data_size);
    m_buffer.size += raw_data_size;

    m_count = (int)m_buffer.size / m_frame_len;
    //
    if (!m_numbers.Resize(sizeof(int)))
        return false;

    memcpy(m_numbers.data + m_numbers.size, &number, sizeof(int));
    m_numbers.size += sizeof(int);

    return true;
}


void CDLISFrame::AddChannels(DlisValueObjName *object, DlisChannelInfo *channels, int channels_count, int frame_len)
{
    m_obj_key        = *object;
    m_channels       = channels;
    m_channels_count = channels_count;

    m_frame_len      = frame_len;
}

int CDLISFrame::GetNumber(int row)
{
    int number;
    
    number = *(((int *)m_numbers.data) + row);
    return number;
}


char *CDLISFrame::GetColumnName(int column)
{
    char *name;

    name = m_channels[column].obj_name->identifier;
    return name;
}


DlisValueObjName *CDLISFrame::GetObject()
{
    return &m_obj_key;
}


float *CDLISFrame::GetValueFloat(int column, int row, int *dimension)
{
    float  *ret;
    
    ret = (float *)GetValue(column, row, dimension);
    return ret;
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


int CDLISFrame::CountColumns()
{
    return m_channels_count;
}


int CDLISFrame::CountRows()
{
    return m_count;
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

    data = m_buffer.data + m_frame_len * row + channel->offsets;
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
