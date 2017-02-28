#pragma once

#include "windows.h"

class CDLISAllocator
{
private:
    struct PullMemory
    {
        char        *data;
        size_t       len;
        size_t       max_size;
        PullMemory  *next;
    };

    struct PullBase
    {
        size_t         id;
        PullMemory    *memory;
        PullBase      *next;
    };


private:
    size_t            m_pull_id;
    PullBase         *m_pulls;

public:
    CDLISAllocator();
    ~CDLISAllocator();

    //
    size_t         PullInitialize(size_t  max_size);
    char          *PullGet (UINT pull_id, size_t size);
    void           PullFree(UINT pull_id);
    void           PullRelease(PullBase *pull);
    void           PullFreeAll();
    

private:
    char          *PullMemoryGet(PullBase *pull, size_t size);
};
