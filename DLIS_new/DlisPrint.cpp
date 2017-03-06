#include "DlisPrint.h"
#include "stdio.h"

CDLISPrint::CDLISPrint() : m_pull_id(0), m_columns(NULL)
{

}


CDLISPrint::~CDLISPrint()
{

}


void CDLISPrint::Initialize()
{
    m_pull_id = m_allocator.PullCreate(32 * 1024);
}


void CDLISPrint::Shutdown()
{
    m_allocator.PullFreeAll();
}


void CDLISPrint::Print(DlisSet *set)
{
    WalkTreeParams params;

    memset(&params, 0, sizeof(WalkTreeParams));
    params.walk_tree_attr = &CDLISPrint::WalkTreeCount;

    Traversal(set, &params);

    params.flags = 0;
    params.row   = 0;
    params.walk_tree_attr         = &CDLISPrint::WalkTreePrintAttr;
    params.walk_tree_begin_set    = &CDLISPrint::WalkTreeSetBegin;
    params.walk_tree_end_set      = &CDLISPrint::WalkTreeSetEnd;
    params.walk_tree_begin_object = &CDLISPrint::WalkTreeObjectBegin;
    params.walk_tree_end_object   = &CDLISPrint::WalkTreeObjectEnd;
    params.walk_tree_end          = &CDLISPrint::WalkTreeEnd;

    Traversal(set, &params);
}


void CDLISPrint::Traversal(DlisSet *set, WalkTreeParams *params)
{
    size_t          row;
    DlisAttribute  *attr;
    DlisObject     *object;

    if (params->walk_tree_begin_set)
        (this->*params->walk_tree_begin_set)(set, params);

    row           = 0;
    params->flags = FLAG_SET;
    attr          = set->colums;
    while (attr)
    {
        params->row = row;
        if (params->walk_tree_attr)
            (this->*params->walk_tree_attr)(attr, params);

        attr = attr->next;
        row++;
    }

    if (params->walk_tree_end_set)
        (this->*params->walk_tree_end_set)(set, params);


    params->flags = FLAG_OBJECT;
    object        = set->objects;
    while (object)
    {

        if (params->walk_tree_begin_object)
            (this->*params->walk_tree_begin_object)(object, params);

        row  = 0;
        attr = object->attr;
        while (attr)
        {
            params->row = row;
            if (params->walk_tree_attr)
                (this->*params->walk_tree_attr)(attr, params);

            row++;
            attr = attr->next;
        }

        if (params->walk_tree_end_object)
            (this->*params->walk_tree_end_object)(object, params);

        object = object->next;
    }
    if (params->walk_tree_end)
        (this->*params->walk_tree_end)(params);
}


void CDLISPrint::DlisSetPrint(DlisSet *set)
{

}


void CDLISPrint::WalkTreeCount(DlisAttribute *attr, WalkTreeParams *params)
{
    DlisColumns  *found;

    found = ColumnsFind(params->row);
    if (!found)
    {
        found = (DlisColumns *)m_allocator.MemoryGet(m_pull_id, sizeof(DlisColumns));
        if (!found)
            return;

        memset(found, 0, sizeof(DlisColumns));


        DlisColumns **next;

        next = &m_columns;
        while (*next)
        {
            next = &(*next)->next;
        }
        *next = found;
    }

    int len = 0;
    
    if (params->flags == FLAG_SET)
        len = (int)strlen(attr->label) + IDENT_LEFT + IDENT_RIGHT;
    else
    {
        if (attr->value)
            if (attr->code == RC_ASCII || attr->code == RC_IDENT)
                len = (int)strlen(attr->value->data) + IDENT_LEFT + IDENT_RIGHT;
    }

    if (len > found->len)
        found->len = len;
}


void CDLISPrint::WalkTreePrintAttr(DlisAttribute *attr, WalkTreeParams *params)
{
    DlisColumns  *found;

    found = ColumnsFind(params->row);
    if (!found)
        return;

    // выводим таблицу
    char   *str;
    int     len;
    char    format_str[128];
    char    dump_string[1] = { 0 };
    
    if (params->flags == FLAG_SET)
        str = attr->label;
    else
    {
        if ((attr->code == RC_ASCII || attr->code == RC_IDENT) && attr->value)
        {
            str = attr->value->data;
        }
        else
            str = dump_string;
    }

    printf("  ");
    printf("%s", str);

    len = (int)strlen(str);
    sprintf_s(format_str, "%%%ds", found->len - len);
    printf(format_str, "");

    printf("  ");
}


void CDLISPrint::WalkTreeSetBegin(DlisSet *set, WalkTreeParams *params)
{

}


void CDLISPrint::WalkTreeSetEnd(DlisSet *set, WalkTreeParams *params)
{
    printf("\n");
}


void CDLISPrint::WalkTreeObjectBegin(DlisObject *object, WalkTreeParams *params)
{
    printf("\n");
}


void CDLISPrint::WalkTreeObjectEnd(DlisObject *object, WalkTreeParams *params)
{
    //printf("\n");
}


void CDLISPrint::WalkTreeEnd(WalkTreeParams *params)
{
    printf("\n");
}


CDLISPrint::DlisColumns *CDLISPrint::ColumnsFind(size_t column)
{
    size_t       index = 0;
    DlisColumns *ret;

    ret = m_columns;
    while (ret)
    {
        if (index == column)
            return ret;

        index++;
        ret = ret->next;
    }

    return NULL;
}
