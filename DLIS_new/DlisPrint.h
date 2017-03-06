#pragma once

#include "DlisCommon.h"
#include "DlisAllocator.h"

class  CDLISPrint
{
private:
    enum constants
    {
        FLAG_SET     = 0x01,
        FLAG_OBJECT  = 0x02,
        IDENT_LEFT   = 2, 
        IDENT_RIGHT  = 2,
    };

private:
    typedef void (CDLISPrint::*WalkTreeFunc)(DlisAttribute *attr, void *params);

    struct WalkTreeRecord
    {
        size_t         row;
        unsigned int   flags;
    };

    struct DlisColumns
    {
        int       column;
        int       len;

        DlisColumns *next;
    };

    
private:
    CDLISAllocator     m_allocator;
    size_t             m_pull_id;

    DlisColumns       *m_columns;

public:
    CDLISPrint();
    ~CDLISPrint();

    void          Initialize();
    void          Shutdown();

private:
    void          Traversal(DlisSet *set, WalkTreeFunc walk_tree);
    void          DlisSetPrint(DlisSet *set);

    void          TreeWalkCount(DlisAttribute *attr, WalkTreeRecord *params);
    void          TreeWalkPrintAttr(DlisAttribute *attr, WalkTreeRecord *params);

    DlisColumns  *ColumnsFind(size_t column);
};
