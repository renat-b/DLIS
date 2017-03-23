#pragma once

#include    "DlisCommon.h"
#include    "stdint.h"

class CDLISDataConverter
{
private:
    uint8_t    m_buffer[8 * 1024];

protected:
    bool       ReadRawData(void *dst, size_t len);

public:
    CDLISDataConverter();
    ~CDLISDataConverter();

    bool       GetFShort();
};
