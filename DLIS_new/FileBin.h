#pragma once

#include "windows.h"

class CFileBin
{
private:
    HANDLE m_file;

public:
    CFileBin();
    ~CFileBin();

    bool   OpenRead(const char *file_name);
    bool   OpenWrite(const char *file_name);

    bool   Close();
    
    bool   Read(void  *data, DWORD *len);
    bool   Write(void *data, DWORD  len);   

    bool   ReadInt32(int *data);
    bool   WriteInt32(int data);
};

