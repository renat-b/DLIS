#pragma once

#include "DlisCommon.h"
#include "DlisAllocator.h"
#include "DLISParser.h"

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
    struct WalkTreeParams;

    typedef void (CDLISPrint::*WalkTreeAttrFunc)(DlisAttribute *attr, WalkTreeParams *params);
    typedef void (CDLISPrint::*WalkTreeBeginSetFunc)(DlisSet *set, WalkTreeParams *params);
    typedef void (CDLISPrint::*WalkTreeEndSetFunc)(DlisSet *set, WalkTreeParams *params);
    typedef void (CDLISPrint::*WalkTreeObjectBeginFunc)(DlisObject *object, WalkTreeParams *params);
    typedef void (CDLISPrint::*WalkTreeObjectEndFunc)(DlisObject *object, WalkTreeParams *params);
    typedef void (CDLISPrint::*WalkTreeEndFunc)(WalkTreeParams *params);

    struct WalkTreeParams
    {
        WalkTreeAttrFunc          walk_tree_attr;
        WalkTreeBeginSetFunc      walk_tree_begin_set;
        WalkTreeEndSetFunc        walk_tree_end_set;
        WalkTreeObjectBeginFunc   walk_tree_begin_object; 
        WalkTreeObjectEndFunc     walk_tree_end_object;
        WalkTreeEndFunc           walk_tree_end;

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
    CDLISParser       *m_parser;

public:
    CDLISPrint();
    ~CDLISPrint();

    void          Initialize();
    void          Shutdown();
    void          Print(CDLISParser *parser);

private:
    void          Traversal(DlisSet *set, WalkTreeParams *params);
    void          DlisSetPrint(DlisSet *set);

    void          WalkTreeCount(DlisAttribute *attr, WalkTreeParams *params);

    void          WalkTreePrintAttr(DlisAttribute *attr, WalkTreeParams *params);
    void          WalkTreeSetBegin(DlisSet *set, WalkTreeParams *params);
    void          WalkTreeSetEnd(DlisSet *set, WalkTreeParams *params);
    void          WalkTreeObjectBegin(DlisObject *object, WalkTreeParams *params);
    void          WalkTreeObjectEnd(DlisObject *object, WalkTreeParams *params);
    void          WalkTreeEnd(WalkTreeParams *params);

    DlisColumns  *ColumnsFind(size_t column);
};
