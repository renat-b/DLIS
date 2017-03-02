#include "DlisAllocator.h"
#include "new.h"


CDLISAllocator::CDLISAllocator() : m_pull_id(0), m_pulls(NULL)
{

}


CDLISAllocator::~CDLISAllocator()
{

}


size_t CDLISAllocator::PullCreate(size_t max_size)
{
    PullBase *pull;

    pull = new(std::nothrow) PullBase;
    if (!pull)
        return (0);

    pull->id     = m_pull_id + 1;
    pull->next   = NULL;
    pull->chunks = NULL;
    
    pull->chunks = new(std::nothrow) PullChunk;
    if (!pull->chunks)
    {
        delete pull;
        return 0;
    }

    pull->chunks->data = new(std::nothrow) char[max_size];
    if (!pull->chunks->data)
    {
        delete pull->chunks;
        delete pull;
        return 0;
    }

    pull->chunks->len      = 0;
    pull->chunks->max_size = max_size;
    pull->chunks->next     = NULL;
    
    PullBase **next;
    // ищем свободный пул
    next = &m_pulls;
    while (*next)
    {
        next = &(*next)->next;
    }
    // вставляем в конец цепочки пулов, новый только что созданный пул
    *next = pull;

    // теперь увеличиваем pull_id
    m_pull_id++;
    // возвращаем pull id созданного пула
    return pull->id;
}


void CDLISAllocator::PullFree(UINT pull_id)
{
    PullBase *pull, **prev;

    pull = m_pulls;
    prev = &m_pulls;

    while (pull)
    {
        if (pull->id == pull_id)
        {
            *prev = pull->next;
            break;
        }

        prev = &pull->next;
        pull = pull->next;

    }

    if (pull)
    {
        PullRelease(pull);
    }
}


void CDLISAllocator::PullFreeAll()
{
    PullBase *pull, *next;

    pull = m_pulls;
    while (pull)
    {
        next = pull->next;
        PullRelease(pull);
        pull = next;
    }
}


void CDLISAllocator::PullRelease(PullBase *pull)
{
    PullChunk *memory, *next;

    memory = pull->chunks;
    while (memory)
    {
        next = memory->next;
        if (memory->data)
            delete [] memory->data;
        delete memory;
        
        memory = next;
    }

    delete pull;
}


char *CDLISAllocator::MemoryGet(size_t pull_id, size_t size)
{
    PullBase *pull;
    
    pull = m_pulls;
    while (pull)
    {
        if (pull->id == pull_id)
        {
            char *ptr;

            ptr = MemoryChunkGet(pull, size);
            return ptr;
        }
        pull = pull->next;
    }

    return NULL;
}


char *CDLISAllocator::MemoryChunkGet(PullBase *pull, size_t size)
{
    PullChunk **curr, *memory;

    curr = &(pull->chunks);
    while (*curr)
    {
        memory = *curr;
        if ((memory->max_size - memory->len) >= size)
        {
            char    *ptr;

            ptr = memory->data + memory->len;
            memory->len += size;
            return ptr;
        }
        curr   = &memory->next;
        memory = memory->next;
    }
   
    memory = new(std::nothrow) PullChunk;
    if (!memory)
        return NULL;
    
    memory->data = new(std::nothrow) char[pull->chunks->max_size];
    if (!memory->data)
    {
        delete memory;
        return NULL;
    }

    memory->len      = 0;
    memory->max_size = pull->chunks->max_size;
    memory->next     = NULL;

    *curr = memory;

    if (memory->max_size <= size)
    {
        memory->len = size;
        return memory->data;
    }

    return NULL;
}