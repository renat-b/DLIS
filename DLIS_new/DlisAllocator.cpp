#include "DlisAllocator.h"
#include "new.h"

CDLISAllocator::CDLISAllocator() : m_pull_id(0), m_pulls(NULL)
{

}


CDLISAllocator::~CDLISAllocator()
{

}


size_t CDLISAllocator::PullInitialize(size_t max_size)
{
    PullBase *pull;

    pull = new(std::nothrow) PullBase;
    if (!pull)
        return (0);

    pull->id     = m_pull_id + 1;
    pull->next   = NULL;
    pull->memory = NULL;
    
    pull->memory = new(std::nothrow) PullMemory;
    if (!pull->memory)
    {
        delete pull;
        return 0;
    }

    pull->memory->data = new(std::nothrow) char[max_size];
    if (!pull->memory->data)
    {
        delete pull->memory;
        delete pull;
        return 0;
    }

    pull->memory->len      = 0;
    pull->memory->max_size = max_size;
    pull->memory->next     = NULL;
    
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


char *CDLISAllocator::PullGet(UINT pull_id, size_t size)
{
    PullBase *pull;
    
    pull = m_pulls;
    while (pull)
    {
        if (pull->id == pull_id)
        {
            char *ptr;

            ptr = PullMemoryGet(pull, size);
            return ptr;
        }
        pull = pull->next;
    }

    return NULL;
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
    PullMemory *memory, *next;

    memory = pull->memory;
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


char *CDLISAllocator::PullMemoryGet(PullBase *pull, size_t size)
{
    PullMemory **curr, *memory;

    curr = &(pull->memory);

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
   
    memory = new(std::nothrow) PullMemory;
    if (!memory)
        return NULL;
    
    memory->data = new(std::nothrow) char[pull->memory->max_size];
    if (!memory->data)
    {
        delete memory;
        return NULL;
    }

    memory->len      = 0;
    memory->max_size = pull->memory->max_size;
    memory->next     = NULL;

    *curr = memory;

    if (memory->max_size <= size)
    {
        memory->len = size;
        return memory->data;
    }

    return NULL;
}