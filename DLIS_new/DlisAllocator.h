#pragma once

#include "windows.h"

class CDLISAllocator
{
private:
    struct PullChunk
    {
        char          *data;
        size_t         len;
        size_t         max_size;
        PullChunk     *next;
    };

    struct PullBase
    {
        size_t         id;
        PullChunk     *chunks;
        PullBase      *next;
    };


private:
    size_t            m_pull_id;
    PullBase         *m_pulls;

public:
    CDLISAllocator();
    ~CDLISAllocator();

    //
    size_t         PullCreate(size_t  max_size);
    void           PullFree(UINT pull_id);
    void           PullRelease(PullBase *pull);
    void           PullFreeAll();

    char          *MemoryGet (size_t pull_id, size_t size);   

private:
    char          *MemoryChunkGet(PullBase *pull, size_t size);
};