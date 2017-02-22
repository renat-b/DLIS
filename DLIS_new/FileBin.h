#pragma once

#include "windows.h"

class CFileBin
{
private:
    HANDLE      m_file;
    size_t      m_count;

    bool        m_test_mode; 
    bool        m_print_mode;

public:
    CFileBin();
    ~CFileBin();

    bool   OpenRead(const char *file_name);
    bool   OpenWrite(const char *file_name);

    void   SetTestCompareFilesMode(bool test_mode);
    bool   IsCompareFilesMode();

    void   SetPrintMode(bool test_mode);
    bool   IsPrintMode();

    bool   Close();
    
    bool   Read(void  *data, DWORD *len);
    bool   Write(void *data, DWORD  len);   

    bool   ReadInt32(int *data);
    bool   WriteInt32(int data);
};

