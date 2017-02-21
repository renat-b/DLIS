#include "FileBin.h"
#include "windows.h"

CFileBin::CFileBin() : m_file(INVALID_HANDLE_VALUE), m_count(0), m_test_mode(true)
{
}


CFileBin::~CFileBin()
{
}


bool CFileBin::OpenRead(const char *file_name)
{
    if ( !m_test_mode)
        return true;

    if ( !file_name)
        return false;

    Close();

    m_file = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_file == INVALID_HANDLE_VALUE)
        return false;
        
    return true;
}


bool CFileBin::OpenWrite(const char *file_name)
{
    if (!m_test_mode)
        return true;

    if ( !file_name)
        return false;

    Close();

    m_file = CreateFile(file_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_file == INVALID_HANDLE_VALUE)
        return false;
        
    return true;
}


void CFileBin::SetTestMode(bool test_mode)
{
    m_test_mode = test_mode;
}


bool CFileBin::GetTestMode()
{
    return m_test_mode;
}


bool CFileBin::Close()
{
    if (m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);

    m_file  = INVALID_HANDLE_VALUE;
    m_count = 0;

    return true;
}


bool CFileBin::Read(void *data, DWORD *len)
{
    BOOL  r;
    DWORD readed = 0;

    r = ReadFile(m_file, data, *len, &readed, NULL);
    if (r == FALSE)
        return false;
    
    if (readed != *len)
    {
        if (GetLastError() == ERROR_SUCCESS)
        {
            *len = readed;
        }
        else
        {
            return false;
        }
    }
    
    m_count ++;
    return true;
}


bool CFileBin::Write(void *data, DWORD len)
{
    BOOL    r;
    DWORD   writed = 0;

    r = WriteFile(m_file, data, len, &writed, NULL);
    
    if (r == FALSE)
        return false;
    
    if (writed != len)
        return false;
    
    m_count ++;
    return true;
}


bool CFileBin::ReadInt32(int *data)
{
    bool   r;
    DWORD  len;
    
    if (!m_test_mode)
        return true;

    len = sizeof(int);
    r = Read(data, &len);

    if (r)
    {
        if (len != sizeof(int))
            return false;
    }

    return r;
}


bool CFileBin::WriteInt32(int data)
{
    bool r;
    
    if (!m_test_mode)
        return true;

    r = Write(&data, sizeof(int));
    return r;
}