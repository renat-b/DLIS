#include "DLISParser.h"
#include "stdio.h"
#include "stdlib.h"
#include "stddef.h"
#include "assert.h"

CFileBin *g_global_log = NULL;

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


CDLISParser::RepresentaionCodesLenght CDLISParser::s_rep_codes_length[RC_LAST] = 
{
    { RC_FSHORT,  2  },
    { RC_FSINGL,  4  },
    { RC_FSING1,  8  },
    { RC_FSING2,  12 },
    { RC_ISINGL,  4  },
    { RC_VSINGL,  4  },
    { RC_FDOUBL,  8  },
    { RC_FDOUB1,  16 },
    { RC_FDOUB2,  24 },
    { RC_CSINGL,  8  },
    { RC_CDOUBL,  16 },
    { RC_SSHORT,  1  },
    { RC_SNORM,   2  },
    { RC_SLONG,   4  },
    { RC_USHORT,  1  },
    { RC_UNORM,   2  },
    { RC_ULONG,   4  },
    { RC_UVARI,   REP_CODE_VARIABLE_SIMPLE  },
    { RC_IDENT,   REP_CODE_VARIABLE_SIMPLE  },
    { RC_ASCII,   REP_CODE_VARIABLE_SIMPLE  },
    { RC_DTIME,   8  },
    { RC_ORIGIN,  REP_CODE_VARIABLE_SIMPLE  },
    { RC_OBNAME,  REP_CODE_VARIABLE_COMPLEX  },
    { RC_OBJREF,  REP_CODE_VARIABLE_COMPLEX  },
    { RC_ATTREF,  REP_CODE_VARIABLE_COMPLEX  },
    { RC_STATUS,  1  },
    { RC_UNITS,   REP_CODE_VARIABLE_SIMPLE  }
};


CDLISParser::CDLISParser() : m_file(INVALID_HANDLE_VALUE), m_state(STATE_PARSER_FIRST), m_template_attributes_count(0), 
    m_attributes_count(0), m_sets(NULL), m_set(NULL), m_object(NULL), m_attribute(NULL), m_column(NULL),
    m_last_set(NULL), m_last_object(NULL), m_last_column(NULL), m_last_attribute(NULL),
    m_pull_id_strings(0), m_pull_id_objects(0)
{
    m_object_num = 0;

    memset(&m_segment,             0, sizeof(m_segment));
    memset(&m_storage_unit_label,  0, sizeof(m_storage_unit_label));
    memset(&m_file_chunk,          0, sizeof(m_file_chunk));
    memset(&m_visible_record,      0, sizeof(m_visible_record));
    memset(&m_segment_header,      0, sizeof(m_segment_header));
    memset(&m_component_header,    0, sizeof(m_component_header));
    memset(&m_template_attributes, 0, sizeof(m_template_attributes));
}


CDLISParser::~CDLISParser()
{

}


bool CDLISParser::Parse(const char *file_name)
{
    if (!file_name)
        return false;

    // открываем файл
    if (!FileOpen(file_name))
        return false;

    // инициализация внутреннего буфера файла    
    if (!BufferInitialize())
        return false;

    // чтение заголовка DLIS        
    if (!ReadStorageUnitLabel())
        return false;

    // чтение данных DLIS    
    if (!ReadLogicalFiles())
        return false;

    return true;
}


bool CDLISParser::Initialize()
{
    memset(&m_file_chunk, 0, sizeof(m_file_chunk)); 

    m_pull_id_strings = m_allocator.PullCreate(32 * 1024);
    if (m_pull_id_strings == 0)
        return false;

    m_pull_id_objects = m_allocator.PullCreate(128 * 1024);
    if (m_pull_id_objects == 0)
        return false;

    m_set = &m_sets;

    return true;
}


void CDLISParser::Shutdown()
{
    FileClose();

    m_file_chunk.Free();
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));

    m_allocator.PullFreeAll();
}


bool CDLISParser::FileOpen(const char *file_name)
{
    if (!file_name)
        return false;

    FileClose();

    m_file = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_file == INVALID_HANDLE_VALUE)
        return false;
        
    return true;
}


bool CDLISParser::FileClose()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
    return true;
}


bool CDLISParser::FileRead(char *data, DWORD len)
{
    BOOL  r;
    DWORD readed = 0;

    r = ReadFile(m_file, data, len, &readed, NULL);
    if (r == FALSE)
        return false;
    
    if (readed != len)
        return false;

    return true;
}


bool CDLISParser::FileWrite(const char *data, DWORD len)
{
    BOOL    r;
    DWORD   writed = 0;

    r = WriteFile(m_file, data, len, &writed, NULL);
    
    if (r == FALSE)
        return false;
    
    if (writed != len)
        return false;

    return true;
}


UINT64 CDLISParser::FileSeekGet()
{
    UINT64  seek;

    LONG    low_seek  = 0;
    LONG    high_seek = 0;
    
    low_seek = SetFilePointer(m_file, 0, &high_seek, FILE_CURRENT);
    
    if (high_seek)
    {
        seek = ((UINT64)high_seek << 32) | low_seek;
    }
    else
    {
        seek = low_seek;
    } 

    return seek;
}


void CDLISParser::FileSeekSet(UINT64 offset)
{
    LONG  low_seek  = 0;
    LONG  high_seek = 0;

    // проверим в старшем DWORD есть значения?
    if (offset & 0XFFFFFFFF00000000)
    {
        low_seek  = (LONG)(offset  & 0X00000000FFFFFFFF);
        high_seek = (LONG)((offset & 0XFFFFFFFF00000000) >> 32);
    }
    else
    {
        low_seek  = (LONG)(offset & 0X00000000FFFFFFFF);
        high_seek = 0;
    }

    SetFilePointer(m_file, low_seek, &high_seek, FILE_BEGIN);
}


UINT64 CDLISParser::FileSize()
{
    UINT64   size;
    DWORD    high_size = 0;
    DWORD    low_size;

    low_size = GetFileSize(m_file, &high_size);
     
    if (high_size)
    {
        size = ((UINT64)high_size << 32) & 0xFFFFFFFF00000000 | ((UINT64)low_size & 0xFFFFFFFF);
    }
    else
    {
        size = (size_t)low_size;
    }

    return size;
}


void CDLISParser::Big2LittelEndian(void *data, size_t len)
{
    static byte tmp[8];

    if (len > sizeof(tmp))
        return;

    byte *src = (byte *)data + len - 1;
    byte *dst = tmp;

    while (src >= data)
    {
        *(dst++) = *(src--);
    }

    memcpy(data, tmp, len);
}


void CDLISParser::Big2LittelEndianByte(byte *bt)
{
    byte dst = 0;
    byte src = *bt;
    
    for (int i = 0; i < 0xf; i++)
        if ((0x1 << i) & src)
            dst |= (0x80 >> i);

    *bt = dst;
}

/*
*  чтение очередно порции данных из файла
*/
bool CDLISParser::BufferNext(char **data, size_t len)
{
    // если в буфере есть требуемой длины размер, копируем и выходим
    if (len <= m_file_chunk.remaind)      
    {
        *data = m_file_chunk.data + m_file_chunk.pos;

        m_file_chunk.pos       += len;
        m_file_chunk.remaind   -= len;
        return true;
    }

    // данных не хватило, читаем следующую порцию данных
    // если есть остаток данных, копируем его в начало буфера
    if (m_file_chunk.remaind) 
    {
        char *src, *dst;
    
        src = m_file_chunk.data + m_file_chunk.pos;
        dst = m_file_chunk.data;
    
        memmove(dst, src, m_file_chunk.remaind);
        m_file_chunk.size = m_file_chunk.remaind;
    }
    else
        m_file_chunk.size = 0;


    // высчитаем правильный остаток данный который нужно вычитать из файла
    DWORD   amout;
    
    amout = min(FILE_CHUNK, (DWORD)(m_file_chunk.file_remaind));    
    // резервируем память под новые данные
    if (!m_file_chunk.Resize(amout + m_file_chunk.remaind))
        return false;
 
    // вычитываем данные
    if (!FileRead(m_file_chunk.data + m_file_chunk.remaind, amout))
        return false; 

    // изменим счетчики
    m_file_chunk.file_remaind -= amout;
    m_file_chunk.remaind      += amout;
    m_file_chunk.pos           = 0;
    m_file_chunk.size_chunk    = m_file_chunk.remaind;
    
    // отдаем данные размера len,  
    if (len <= m_file_chunk.remaind)      
    {
        *data = m_file_chunk.data;

        m_file_chunk.pos      += len;
        m_file_chunk.remaind  -= len;
        return true;
    }

    // если не получается, значит произошла ошибка 
    return false;
}


bool CDLISParser::BufferInitialize()
{
    m_file_chunk.Free();
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));
    
    m_file_chunk.file_remaind = FileSize();
    return true;
}

/*
* провера конца файла
*/
bool CDLISParser::BufferIsEOF()
{
    return (m_file_chunk.remaind == 0 && m_file_chunk.file_remaind == 0);
}


/*
*  получаем следующий сегмент данных 
*/
bool CDLISParser::SegmentGet()
{
    bool r = true;
    // если данных не хватает в текущем visible record, читаем следующую visible record
    if (m_visible_record.current >= m_visible_record.end)
        r = VisibleRecordNext();

    if (!r)
        return false;

    // указатель на заголовок
    m_segment_header = *(SegmentHeader *)m_visible_record.current; 

    //  получим полный размер сегмента и его атрибуты
    Big2LittelEndian(&(m_segment_header.length), sizeof(m_segment_header.length));
    Big2LittelEndianByte(&m_segment_header.attributes);
    

    short  size_header = (short)(offsetof(SegmentHeader, length_data));
    // рассчитаем актуальный размер сегмента (содержащий данные  без метаданных)
    m_segment_header.length_data = m_segment_header.length - size_header;

    if (m_segment_header.attributes & Trailing_Length)
        m_segment_header.length_data -= sizeof(short);

    if (m_segment_header.attributes & Checksum)
        m_segment_header.length_data -= sizeof(short);
    
    if (m_segment_header.attributes & Padding)
    {
        byte  *pad_len;
        char  *data;  
        // рассчитаем количество padding (выравнивающих) символов для этого :
        // прочитаем значение по адресу 
        // начальное смещение + размер данных - 1 байт (т.к. в этом месте находится значение количества padding байт)
        // по этому адресу содержится количество padding символов
        data = m_visible_record.current + size_header;
        
        pad_len = (byte *)(data) + m_segment_header.length_data - sizeof(byte);
        assert((data + (*pad_len)) <= m_visible_record.end);
        m_segment_header.length_data -= *pad_len;
    }
    

    // выставим значения сегмента, его актуальный размер, начало и конец
    m_segment.current = m_visible_record.current + size_header;
    m_segment.end     = m_segment.current + m_segment_header.length_data;
    m_segment.len     = m_segment_header.length_data;

    m_visible_record.current += m_segment_header.length;

    return true;
}

/*
*  обрабатываем сегмент
*/
bool CDLISParser::SegmentProcess()
{
    bool r = true;

    if (m_segment_header.attributes & Logical_Record_Structure)
    {
        // первый сегмент в логическом файле 
        if (m_segment_header.type == FHLR)
        {
        }

        // продолжение сегмента (второй и тд сегмент) в списке сегментов
        if (m_segment_header.attributes & Successor)
        {
        }
        

        // последовательно вычитываем компоненты
        r = ComponentRead();
        while (r)
        {
            // конец сегмента, выходим для получения новой порции данных
            if (m_segment.current >= m_segment.end)
                break;

            r = ComponentRead();
        }


    }
   
    return r;

}

/*
*  получаем следующую visible record  
*/
bool CDLISParser::VisibleRecordNext()
{
    // обнулим текущую структуру
    m_visible_record.current = NULL;
    m_visible_record.end     = NULL;
    m_visible_record.len     = 0;    

    char                 *data;
    VisibleRecordHeader  *header;

    // читаем заголовок
    bool r = BufferNext(&data, sizeof(VisibleRecordHeader));

    if (r) 
    {
        header = (VisibleRecordHeader *)data;
        Big2LittelEndian(&((*header).length), sizeof(header->length));
    }

    // читаем visible record размером указанным в заголовке 
    if (r)
    {
        char *record;
        
        m_visible_record.len = header->length - sizeof(VisibleRecordHeader);
        r = BufferNext(&record, m_visible_record.len);            

        if (r)
        {
            m_visible_record.current = record;
            m_visible_record.end     = record + m_visible_record.len; 
        }
    }

    return r;
}

/*
*  читаем заголовок DLIS
*/
bool CDLISParser::ReadStorageUnitLabel()
{
    bool    r;
    char   *data;

    r = BufferNext(&data, sizeof(m_storage_unit_label));
    
    if (r)
        m_storage_unit_label = *(StorageUnitLabel *)data;

    return r;
}


/*
*  читаем последовательно сегменты
*/
bool CDLISParser::ReadLogicalFiles()
{
    bool r = SegmentGet();
        
    while (r)
    {
        //
        if (BufferIsEOF()) 
            break;

        r = SegmentProcess(); 

        if (r)
            r = SegmentGet();
    }

    return r;
}

/*
* читаем компонет DLIS и обрабатываем его 
*/
bool CDLISParser::ComponentRead()
{
    bool r;
    
    r = ComponentHeaderGet();
    if (r)
    {
        if (m_component_header.role == Absent_Attribute || m_component_header.role == Attribute || m_component_header.role == Invariant_Attribute)             
        {
            r = ReadAttribute();
        }
        else if (m_component_header.role == Object)
        {
            r = ReadObject();
        }
        else if (m_component_header.role == Redundant_Set || m_component_header.role == Replacement_Set || m_component_header.role == Set)
        {
            r = ReadSet();
        }
    }

    return r;
}

/*
*
*/
bool CDLISParser::ComponentHeaderGet()
{
    byte desc = *(byte *)m_segment.current;
   
    if (g_global_log->IsCompareFilesMode())
    {
        int test_component_header = 0;
        int component_header      = desc;

        g_global_log->ReadInt32(&test_component_header);
         
        assert(test_component_header == component_header);   
    }

    // получим роль и формат Component-а
    // для получения role сбросим вехрние 5 бит
    // для получние format сбросим нижние 3 бита
    m_component_header.role   = (desc & 0xE0) >> 5;
    m_component_header.format = (desc & 0x1F);
    Big2LittelEndianByte(&m_component_header.format);

    m_segment.current += sizeof(byte);
    m_segment.len     -= sizeof(byte);

    assert(m_segment.current <= m_segment.end);

    return true;
}

/*
* читаем сырые данные из буфера
*/
bool CDLISParser::ReadRawData(void *dst, size_t len)
{
    // проверяем хватает ли нам данных в сегменте?
    if (len > m_segment.len)
    {
        // если не хватает, проверяем это не последний сегмент в группе сегментов? Если последний, то ошибка
        if ( !(m_segment_header.attributes & Successor))
        {
            assert(false); 
            return false;
        }

        // вычитываем остаток данных из текущего сегмента
        size_t old_len = m_segment.len;

        memcpy(dst, m_segment.current, old_len);
        // получаем новый сегмент
        if ( !SegmentGet())
            return false;

        // до-копируем остаток требуемых данных из нового сегмента 
        memcpy( ((char *)dst) + old_len, m_segment.current, len - old_len);

        m_segment.current += len - old_len;
        m_segment.len     -= len - old_len;

        return true;
    }

    // данных хватает, просто вычитываем требуемый объем данных
    memcpy(dst, m_segment.current, len);
    
    m_segment.current += len;
    m_segment.len     -= len;

    return true;
}

/*
* вычитываем данные по representation code
*/
bool CDLISParser::ReadCodeSimple(RepresentaionCodes code, void **dst, size_t *len, int count /*= 1*/)
{
    // резервируем данные для быстрого доступа 8 килобайт
    static byte buf[8 * Kb] = { 0 };
    int    type_len;

    memset(&buf[0], 0, sizeof(buf));

    // получаем размер representation code
    type_len = s_rep_codes_length[code - 1].length;

    // сложные данные функция обрабатывать не умеет 
    if (type_len == REP_CODE_VARIABLE_COMPLEX)
        assert(false);

    // если размер известен, просто копируем их в буфер и выходим
    if (type_len > 0)
    {
        ReadRawData(buf, type_len);
        
        *dst = buf;
        *len = type_len;

        return true;
    }

    // читаем вариативные данные
    switch (code)
    {
        case RC_IDENT:
        case RC_ASCII:
        case RC_UNITS:
            {
                size_t    count;
                void     *ptr;
                
                UINT      str_len = 0;
                // в зависимости от code сначала читаем размер данных        
                if (code == RC_ASCII)
                    ReadCodeSimple(RC_UVARI,  &ptr, &count);
                else
                    ReadCodeSimple(RC_USHORT, &ptr, &count);

                // получаем размер данных в байтах
                assert(count <= sizeof(str_len));
                memcpy(&str_len, ptr, count);
                // читаем сами данные
                ReadRawData(buf, str_len);
                
                *dst = buf;
                *len = str_len;
            }
            break;

        case RC_UVARI:
        case RC_ORIGIN:
            {
                byte  var_len;

                // определим размер данных
                // верхние 2 бита определяют полное количество байт которое надо считать,
                // в 6 нижних, лежат данные 
                memset(&buf[0], 0, sizeof(buf));
                ReadRawData(buf, 1);

                // определим полный размер в байтах который нужно считать               
                if ((*buf & 0xC0) == 0xC0)
                {
                    var_len = 4;
                    buf[0] &= ~0xC0;
                }
                else if ((*buf & 0x80) == 0x80)
                {
                    var_len = 2;
                    buf[0] &= ~0x80;
                }
                else
                    var_len = 1;

                // дочитаем данные
                if (var_len > 1)
                    ReadRawData(&buf[1], var_len - 1);

                // конвертируем считанные данные 
                Big2LittelEndian(buf, var_len);

                *len = var_len;
                *dst = buf;
            }
            break;

        default:
            assert(false);
            break;
    }

    return true;
}

/*
* 
*/
bool CDLISParser::ReadCodeComplex(RepresentaionCodes code, void *dst)
{
    char    *src;
    size_t   len;

    switch (code)
    {
        case RC_OBNAME:
            {
                DlisValueObjName  *value;

                value = (DlisValueObjName *)dst;

                ReadCodeSimple(RC_ORIGIN, (void **)&src, &len);
                memcpy(&value->origin_reference, src, len);

                ReadCodeSimple(RC_USHORT, (void **)&src, &len);
                memcpy(&value->copy_number, src, len); 

                ReadCodeSimple(RC_IDENT, (void **)&src, &len);
                value->identifier = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                strcpy_s(value->identifier, len + 1, src);
            }
            break;

        case RC_OBJREF:
            {
                DlisValueObjRef  *value;

                value = (DlisValueObjRef *)dst;

                ReadCodeSimple(RC_IDENT, (void **)&src, &len);
                value->object_type = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                strcpy_s(value->object_type, len + 1, src);

                ReadCodeComplex(RC_OBNAME, (void **)&value->object_name);
            }
            break;

        case RC_ATTREF:
            {
                DlisValueAttRef *value;
                
                value = (DlisValueAttRef *)dst;

                ReadCodeSimple(RC_IDENT, (void **)&src, &len);
                value->object_type = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                strcpy_s(value->object_type, len + 1, src);
                
                ReadCodeComplex(RC_OBNAME,(void **)&value->object_name);

                ReadCodeSimple(RC_IDENT, (void **)&src, &len);
                value->attribute_label = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                strcpy_s(value->attribute_label, len + 1, src);
            }
            break;

        default:
            break;
    }

    return true;
}

/*
* читаем атрибут объекта или шаблона (template)
*/
bool CDLISParser::ReadAttribute()
{
    char      *val;
    size_t     len;
    UINT       count            = 1;
    RepresentaionCodes rep_code = RC_IDENT;

    // если первый атрибут, определим это атрибут объекта или шаблона
    if (m_state == STATE_PARSER_SET)
    {
        m_state = STATE_PARSER_TEMPLATE_ATTRIBUTE;
    }
    if (m_state == STATE_PARSER_OBJECT)
    {
        m_state = STATE_PARSER_ATTRIBUTE;
    }


    DlisAttribute *attr;
    
    attr = (DlisAttribute *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisAttribute));
    memset(attr, 0, sizeof(DlisAttribute));
    attr->count = 1;

    if (m_state == STATE_PARSER_TEMPLATE_ATTRIBUTE)
        ColumnAdd(attr);
    else
        AttributeAdd(attr);

    // последовательно читаем свойства атрибута
    if (m_component_header.format & TypeAttribute::TypeAttrLable)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);
        attr->label = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        strcpy_s(attr->label, len + 1, val);
    }

    if (m_component_header.format & TypeAttribute::TypeAttrCount)
    {
        ReadCodeSimple(RC_UVARI, (void **)&val, &len);
        memcpy(&attr->count, val, len);
    }

    if (m_component_header.format & TypeAttribute::TypeAttrRepresentationCode)
    {
        ReadCodeSimple(RC_USHORT, (void **)&val, &len);
        memcpy(&attr->code, val, len);
    }
    else
    {
        if (m_state == STATE_PARSER_ATTRIBUTE)
        {
            DlisAttribute *column;

            column = AttrRepresentationCodeFind(m_last_set, m_last_object, attr);
            if (column)
                attr->code = column->code;
            else
                attr->code = RC_UNDEFINED;
        }
        else
        {
                attr->code = RC_ASCII;
        }
    } 
    
    if (m_component_header.format & TypeAttribute::TypeAttrUnits)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);

        attr->units = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        strcpy_s(attr->units, len + 1, val);
    }

    if (m_component_header.format & TypeAttribute::TypeAttrValue)
    {
        int        type;
        DlisValue *attr_val;

        attr->value = (DlisValue *)m_allocator.MemoryGet(m_pull_id_strings, attr->count * sizeof(DlisValue));
        memset(attr->value, 0, sizeof(DlisValue));

        type        = s_rep_codes_length[attr->code - 1].length;
        attr_val    = attr->value;

        for (size_t i = 0; i < attr->count; i++)
        {
            ReadAttributeValue(attr_val, attr->code, type);
            attr_val++;
        }
    }

    return true;
}


bool CDLISParser::ReadAttributeValue(DlisValue *attr_val, RepresentaionCodes code, int type)
{
    char    *val;
    size_t   len;

    if (type > 0)
    {
        ReadCodeSimple(code, (void **)&val, &len);
        attr_val->data = m_allocator.MemoryGet(m_pull_id_strings, type);
        memcpy(attr_val->data, val, len);
    }
    else if (type == REP_CODE_VARIABLE_SIMPLE)
    {
        ReadCodeSimple(code, (void **)&val, &len);
        attr_val->data = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        strcpy_s(attr_val->data, len + 1, val);
    }
    else if (type == REP_CODE_VARIABLE_COMPLEX)
    {
        switch (code)
        {
            case RC_OBNAME:
                {
                    DlisValueObjName *obj_name;

                    obj_name = (DlisValueObjName *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueObjName));
                    ReadCodeComplex(code, (DlisValueObjName *)obj_name);
                    attr_val->data = (char *)obj_name;
                }
                break;

            case RC_OBJREF:
                {
                    DlisValueObjRef *obj_ref;

                    obj_ref = (DlisValueObjRef *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueObjRef));
                    ReadCodeComplex(code, (DlisValueObjRef *)obj_ref);
                    attr_val->data = (char *)obj_ref;
                }
                break;

            case RC_ATTREF:
                {
                    DlisValueAttRef *att_ref;

                    att_ref = (DlisValueAttRef *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueAttRef));
                    ReadCodeComplex(code, (DlisValueAttRef *)att_ref);
                    attr_val->data = (char *)att_ref;
                }
                break;
        }
    }

    return true;
}


void CDLISParser::SetAdd(DlisSet *set)
{
    *m_set   = set;

    m_column = &(set->colums);
    m_object = &(set->objects);
    m_set    = &(*m_set)->next;
}


void CDLISParser::ObjectAdd(DlisObject *obj)
{
    *m_object   = obj;

    m_attribute = &(obj->attr);
    m_object    = &(*m_object)->next;
}


void CDLISParser::ColumnAdd(DlisAttribute *column)
{
    *m_column = column;
    m_column  = &(*m_column)->next;
}


void CDLISParser::AttributeAdd(DlisAttribute *attribute)
{
    *m_attribute = attribute;
    m_attribute  = &(*m_attribute)->next;
}

/*
* 
*/
DlisAttribute *CDLISParser::AttrRepresentationCodeFind(DlisSet *set, DlisObject *object, DlisAttribute *attr)
{
    DlisAttribute *ret = NULL;
    DlisAttribute *attribute;

    ret       = set->colums;
    attribute = object->attr;
    while (ret)
    {
        if (attr == attribute)
            break;

        attribute = attribute->next;
        ret       = ret->next;
    }

    return ret;
}

/*
* читаем объект
*/
bool CDLISParser::ReadObject()
{
    m_state = STATE_PARSER_OBJECT; 
    
    m_last_object = (DlisObject *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisObject));
    memset(m_last_object, 0, sizeof(DlisObject));

    ObjectAdd(m_last_object);

    if (m_component_header.format & TypeObject::TypeObjectName)
    {
        ReadCodeComplex(RC_OBNAME, (void **)&m_last_object->name);
    }

    return true;
}

/*
*  читаем Set
*/
bool CDLISParser::ReadSet()
{
    char       *val;
    size_t      len;

    m_state                     = STATE_PARSER_SET;
    m_object_num                = 0;
    m_attributes_count          = 0;
    m_template_attributes_count = 0; 
    memset(&m_template_attributes, 0, sizeof(m_template_attributes));

    m_last_set = (DlisSet *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisSet));
    if (!m_last_set)
        return false;

    memset(m_last_set, 0, sizeof(DlisSet));
    SetAdd(m_last_set);

    if (m_component_header.format & TypeSet::TypeSetType)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);
        
        m_last_set->type = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        if (!m_last_set->type)
            return false;
        
        strcpy_s(m_last_set->type, len + 1, val);
    }

    if (m_component_header.format & TypeSet::TypeSetName)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);

        m_last_set->name = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        if (!m_last_set->name)
            return false;
        
        strcpy_s(m_last_set->name, len + 1, val);
    }
    return true;
}

/*
* принтуем код объекта в его строковое описание (для отладки)
*/
void CDLISParser::DebugPrintRepCode(RepresentaionCodes code, char *str_rep_code, size_t size)
{
    
    str_rep_code[0] = 0;

    switch (code)
    {
    case    RC_FSHORT:
        strcpy_s(str_rep_code, size, "FSHORT");
        break;

    case    RC_FSINGL:
        strcpy_s(str_rep_code, size, "FSINGL");
        break;

    case    RC_FSING1:
        strcpy_s(str_rep_code, size, "FSING1");
        break;

    case    RC_FSING2:
        strcpy_s(str_rep_code, size, "FSING2");
        break;

    case    RC_ISINGL:
        strcpy_s(str_rep_code, size, "ISINGL");
        break;

    case    RC_VSINGL:
        strcpy_s(str_rep_code, size, "VSINGL");
        break;

    case    RC_FDOUBL:
        strcpy_s(str_rep_code, size, "FDOUBL");
        break;

    case    RC_FDOUB1:
        strcpy_s(str_rep_code, size, "FDOUB1");
        break;

    case    RC_FDOUB2:
        strcpy_s(str_rep_code, size, "FDOUB2");
        break;

    case    RC_CSINGL:
        strcpy_s(str_rep_code, size, "CSINGL");
        break;

    case    RC_CDOUBL:
        strcpy_s(str_rep_code, size, "CDOUBL");
        break;

    case    RC_SSHORT:
        strcpy_s(str_rep_code, size, "SSHORT");
        break;

    case    RC_SNORM:
        strcpy_s(str_rep_code, size, "SNORM");
        break;

    case    RC_SLONG:
        strcpy_s(str_rep_code, size, "SLONG");
        break;

    case    RC_USHORT:
        strcpy_s(str_rep_code, size, "USHORT");
        break;

    case    RC_UNORM:
        strcpy_s(str_rep_code, size, "UNORM");
        break;

    case    RC_ULONG:
        strcpy_s(str_rep_code, size, "ULONG");
        break;

    case    RC_UVARI:
        strcpy_s(str_rep_code, size, "UVARI");
        break;

    case    RC_IDENT:
        strcpy_s(str_rep_code, size, "IDENT");
        break;

    case    RC_ASCII:
        strcpy_s(str_rep_code, size, "ASCII");
        break;

    case    RC_DTIME:
        strcpy_s(str_rep_code, size, "DTIME");
        break;

    case    RC_ORIGIN:
        strcpy_s(str_rep_code, size, "ORIGIN");
        break;

    case    RC_OBNAME:
        strcpy_s(str_rep_code, size, "OBNAME");
        break;

    case    RC_OBJREF:
        strcpy_s(str_rep_code, size, "OBJREF");
        break;

    case    RC_ATTREF:
        strcpy_s(str_rep_code, size, "ATTREF");
        break;

    case    RC_STATUS:
        strcpy_s(str_rep_code, size, "STATUS");
        break;

    case    RC_UNITS:
        strcpy_s(str_rep_code, size, "UNITS");
        break;

    default: 
        break;
    }

}


void CDLISParser::DebugPrintAttrCode(UINT attr_code, char *str_attr_code, size_t size)
{
    str_attr_code[0] = 0;   
   
    switch (attr_code)
    {
        case Absent_Attribute:
            strcpy_s(str_attr_code, size, "Absent Attribute");
            break;
    
        case Attribute: 
            strcpy_s(str_attr_code, size, "Attribute");
            break;
    
        case Invariant_Attribute: 
            strcpy_s(str_attr_code, size, "Invariant Attribute");
            break;
    
        case Object:
            strcpy_s(str_attr_code, size, "Object");
            break;
    
        case Redundant_Set:
            strcpy_s(str_attr_code, size, "Redundant Set");
            break;
    
        case Replacement_Set:
            strcpy_s(str_attr_code, size, "Replacement Set");
            break;

        case Set:
            strcpy_s(str_attr_code, size, "Set");
            break;

        default:
            break;
    }

}
