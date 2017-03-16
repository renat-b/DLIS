#include "MemoryBuffer.h"
#include "windows.h"
#include "new.h"

bool MemoryBuffer::Resize(size_t new_max_size)
{

    // если размера буфера хватает, выходим - выделять ничего не надо
    if (new_max_size < max_size)
        return true;
    
    char   *buf = NULL;
    size_t  cap = new_max_size / 4;

    // ограничим размер нового буфера по минимальному и максимальному значению
    if (cap < 16)    
        cap = 16;
    
    if (cap > 1024 * 1024)
        cap  = 32 * 1024;

    // новая емкость буфера
    cap += new_max_size;
    
    // выделим память под буфер
    buf = new(std::nothrow) char[cap];
    if (!buf)
        return false;
    
    // копируем старые данные
    memcpy(buf, data, size);
    max_size = cap;
     
    // освободим старую память и запомним новый буфер
    delete data;
    data = buf;

    return true;
}


void MemoryBuffer::Free()
{
    if (data)
        delete data;
    
    data     = NULL;
    size     = 0;
    max_size = 0;
}

