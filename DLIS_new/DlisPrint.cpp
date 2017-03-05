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


void CDLISPrint::Traversal(DlisSet *set, WalkTreeFunc walk_tree)
{
    size_t           row;
    DlisAttribute   *attr;
    DlisObject      *object;
    WalkTreeRecord   record;

    row          = 0;
    record.flags = FLAG_SET;
    attr         = set->colums;
    while (attr)
    {
        record.row = row;
        (this->*walk_tree)(attr, &record);

        attr = attr->next;
        row++;
    }


    record.flags = FLAG_OBJECT;
    object       = set->objects;
    while (object)
    {
        row  = 0;
        attr = object->attr;
        while (attr)
        {
            record.row = row;
            (this->*walk_tree)(attr, &record);

            row++;
            attr = attr->next;
        }

        object = object->next;
    }
}


void CDLISPrint::DlisSetPrint(DlisSet *set)
{

}


void CDLISPrint::TreeWalkCount(DlisAttribute *attr, WalkTreeRecord *params)
{
    DlisColumns  *found;

    found = ColumnsFind(params->row);
    if (!found)
    {
        found = (DlisColumns *)m_allocator.MemoryGet(m_pull_id, sizeof(DlisColumns));
        if (!found)
            return;


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


void CDLISPrint::TreeWalkPrintAttr(DlisAttribute *attr, WalkTreeRecord *params)
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
