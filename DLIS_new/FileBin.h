#pragma once

#include "windows.h"

class CFileBin
{
private:
    HANDLE      m_file;
    size_t      m_count;
    bool        m_test_mode; 

public:
    CFileBin();
    ~CFileBin();

    bool   OpenRead(const char *file_name);
    bool   OpenWrite(const char *file_name);
    void   SetTestMode(bool test_mode);
    bool   GetTestMode();

    bool   Close();
    
    bool   Read(void  *data, DWORD *len);
    bool   Write(void *data, DWORD  len);   

    bool   ReadInt32(int *data);
    bool   WriteInt32(int data);
};

