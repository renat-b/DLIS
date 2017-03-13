#include "DLISParser.h"
#include "stdio.h"
#include "stdlib.h"
#include "stddef.h"
#include "assert.h"
#include "DlisPrint.h"

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


CDLISParser::CDLISParser() : m_file(INVALID_HANDLE_VALUE), m_state(STATE_PARSER_FIRST), 
    m_sets(NULL), m_set_tail(NULL), m_object_tail(NULL), m_attribute_tail(NULL), m_column_tail(NULL),m_frame_tail(NULL),
    m_last_set(NULL), m_last_root_set(NULL), m_last_object(NULL), m_last_column(NULL), m_last_attribute(NULL),
    m_pull_id_strings(0), m_pull_id_objects(0), m_pull_id_frame_data(0), 
    m_last_frame(NULL), m_frame_data(NULL)
{
    memset(&m_segment,             0, sizeof(m_segment));
    memset(&m_storage_unit_label,  0, sizeof(m_storage_unit_label));
    memset(&m_file_chunk,          0, sizeof(m_file_chunk));
    memset(&m_visible_record,      0, sizeof(m_visible_record));
    memset(&m_segment_header,      0, sizeof(m_segment_header));
    memset(&m_component_header,    0, sizeof(m_component_header));
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

    // CDLISPrint debug_print;

    // debug_print.Initialize();
    // debug_print.Print(this);
    // debug_print.Shutdown();

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

    m_pull_id_frame_data = m_allocator.PullCreate(32 * Kb);
    if (m_pull_id_frame_data == 0)
        return false;

    m_set_tail = &m_sets;

    return true;
}


void CDLISParser::Shutdown()
{
    FileClose();

    m_file_chunk.Free();
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));

    m_allocator.PullFreeAll();
}


char *CDLISParser::AttrGetString(DlisAttribute *attr, char *buf, size_t buf_len)
{
    if (!attr)
        return NULL;

    if (!buf || buf_len == 0)
        return NULL;

    buf[0] = 0;

    if (!attr->value)
        return buf;

    switch (attr->code)
    {
        case RC_ASCII:
        case RC_IDENT:
            strcpy_s(buf, buf_len, attr->value->data);
            break;

        case RC_ORIGIN:
        case RC_UVARI:
            {
                char   *src; 
                size_t  len_byte;
                int     val = 0;
                // в первом байте содержится размер, далее данные в байтах
                len_byte = *(byte *)attr->value->data;
                src = attr->value->data + sizeof(byte);                
                memcpy(&val, src, len_byte); 
                _itoa_s(val, buf, buf_len, 10);
            }
            break;

        case RC_SSHORT:                        // 1 	Short signed integer
        case RC_SNORM:                         // 2 	Normal signed integer
        case RC_SLONG:                         // 4 	Long signed integer
        case RC_USHORT:                        // 1 	Short unsigned integer
        case RC_UNORM:                         // 2 	Normal unsigned integer
        case RC_ULONG:                         // 4 	Long unsigned integer
            {
                unsigned int val = 0;
                int          len = 1; 
               
                len = s_rep_codes_length[attr->code - 1].length; 
                
                memcpy(&val, attr->value->data, len);
                if (attr->code >= RC_SSHORT && attr->code <= RC_SLONG)
                    _itoa_s(val, buf, buf_len, 10);
                else
                    _ltoa_s((long)val, buf, buf_len, 10);
            }
            break; 

        case RC_FSINGL:                     
        case RC_FDOUBL:
            {
                float   val;
                double  val_d;

                if (attr->code == RC_FSINGL)
                {
                    memcpy(&val, attr->value->data, sizeof(float));
                    val_d = val;
                }
                else
                {
                    memcpy(&val_d, attr->value->data, sizeof(double));
                }
                sprintf_s(buf, buf_len, "%.2f", val_d);
            }
            break;

        case RC_UNITS:
            strcpy_s(buf, buf_len, attr->value->data);
            break;

        default:
            break;
    }

    if (attr->units)
    {
        strcat_s(buf, buf_len, " ");
        strcat_s(buf, buf_len, attr->units);
    }

    return buf;
}


int CDLISParser::AttrGetInt(DlisAttribute *attr)
{
    char   buf[16] = { 0 };
    char  *ptr;
    
    ptr = AttrGetString(attr, buf, sizeof(buf));
    if (!ptr)
        return 0;


    int  r;

    r = atoi(ptr);
    return r;
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
    bool r;
    
    r = m_visible_record.end <= m_visible_record.current;
    if (r)
        r = m_file_chunk.remaind == 0 && m_file_chunk.file_remaind == 0;

    return (r);
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
    else
    {
        if ( !(m_segment_header.attributes & Predecessor))
            r = ReadIndirectlyFormattedLogicalRecord();

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
    bool r = true;
        
    while (r)
    {
        r = SegmentGet();

        if (r)
            r = SegmentProcess();

        if (r)
            if (BufferIsEOF())
                break;
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
bool CDLISParser::ReadCodeSimple(RepresentationCodes code, void **dst, size_t *len)
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
        switch(code)
        {
            case  RC_SSHORT: 
            case  RC_SNORM:  
            case  RC_SLONG:  
            case  RC_USHORT: 
            case  RC_UNORM:  
            case  RC_ULONG:  
            case  RC_FSINGL:
            case  RC_FDOUBL:
                Big2LittelEndian(buf, type_len);
                break;

            default:
                break;
        }
        
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
                size_t   count;
                void    *ptr;
                UINT     str_len = 0;
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

                *len = str_len;
                *dst = StringTrim((char *)buf, len);
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
bool CDLISParser::ReadCodeComplex(RepresentationCodes code, void *dst)
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
                DlisValueAttRef  *value;
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
*
*/
bool CDLISParser::ReadIndirectlyFormattedLogicalRecord()
{                
    static char       buf[128];
    char             *src;
    size_t            len, len_str;
    DlisValueObjName  obj_name = { 0 };
    

    ReadCodeSimple(RC_ORIGIN, (void **)&src, &len);
    memcpy(&obj_name.origin_reference, src, len);

    ReadCodeSimple(RC_USHORT, (void **)&src, &len);
    memcpy(&obj_name.copy_number, src, len); 

    ReadCodeSimple(RC_IDENT, (void **)&src, &len_str);
    strcpy_s(buf, len_str + 1, src);
    obj_name.identifier = buf;


    // DlisFrameData *frame;

    // frame = (DlisFrameData *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisFrameData));
    // if (!frame)
    //     return false;

    // memset(frame, 0, sizeof(DlisFrameData));

    // if (m_last_frame && ObjectNameCompare(&obj_name, &(m_last_frame->obj_name)))
    // {
    //     frame->obj_name.identifier = m_last_frame->obj_name.identifier;
    // }
    // else
    // {
    //     frame->obj_name.identifier = m_allocator.MemoryGet(m_pull_id_strings, len_str + 1);

    //     strcpy_s(frame->obj_name.identifier, len_str + 1, obj_name.identifier);
    //     frame->obj_name.copy_number      = obj_name.copy_number;
    //     frame->obj_name.origin_reference = obj_name.origin_reference;
    // }

    // FrameAdd(frame);

    FrameData *frame;

    frame = FrameDataFind(&obj_name);
    if (!frame)
    {    
        frame = FrameDataBuild(&obj_name);
        if (!frame)
            return false;
    }

    FrameDataParse(frame);


    return true;
}

/*
*
*/
bool CDLISParser::ReadAttributeValue(DlisValue *attr_val, RepresentationCodes code, int type)
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
        switch(code)
        {
            case RC_UVARI:
            case RC_ORIGIN:
                attr_val->data = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                *(byte *)(attr_val->data) = (byte)len;
                memcpy(attr_val->data + 1, val, len);
                break;

            default:
                attr_val->data = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
                strcpy_s(attr_val->data, len + 1, val);
                break;

        }
    }
    else if (type == REP_CODE_VARIABLE_COMPLEX)
    {
        switch (code)
        {
            case RC_OBNAME:
                {
                    DlisValueObjName *obj_name;

                    obj_name = (DlisValueObjName *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueObjName));
                    memset(obj_name, 0, sizeof(DlisValueObjName));

                    ReadCodeComplex(code, (DlisValueObjName *)obj_name);
                    attr_val->data = (char *)obj_name;
                }
                break;

            case RC_OBJREF:
                {
                    DlisValueObjRef *obj_ref;

                    obj_ref = (DlisValueObjRef *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueObjRef));
                    memset(obj_ref, 0, sizeof(DlisValueObjRef));

                    ReadCodeComplex(code, (DlisValueObjRef *)obj_ref);
                    attr_val->data = (char *)obj_ref;
                }
                break;

            case RC_ATTREF:
                {
                    DlisValueAttRef *att_ref;

                    att_ref = (DlisValueAttRef *)m_allocator.MemoryGet(m_pull_id_strings, sizeof(DlisValueAttRef));
                    memset(att_ref, 0, sizeof(DlisValueAttRef));

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
    if (set->type_set == FHLR)
    {
        m_set_tail = &m_sets;
        
        while (*m_set_tail)
        {
            m_set_tail = &(*m_set_tail)->next;
        }

        *m_set_tail  = set;
        m_set_tail   = &(*m_set_tail)->childs;

        m_frame_tail = &(set)->frame;

		m_last_root_set = set;
    }
    else
    {
        *m_set_tail = set;
        m_set_tail  = &(*m_set_tail)->next; 
    }

    m_last_set      = set;
    m_column_tail   = &(set->colums);
    m_object_tail   = &(set->objects);
}


void CDLISParser::ObjectAdd(DlisObject *obj)
{
    *m_object_tail   = obj;
	m_last_object    = obj;
	
    m_attribute_tail = &(obj->attr);
    m_object_tail    = &(*m_object_tail)->next;
}


void CDLISParser::ColumnAdd(DlisAttribute *column)
{
    *m_column_tail = column;
    m_column_tail  = &(*m_column_tail)->next;
}


void CDLISParser::AttributeAdd(DlisAttribute *attribute)
{
    *m_attribute_tail = attribute;
    m_attribute_tail  = &(*m_attribute_tail)->next;
}

/*
* 
*/
void CDLISParser::FrameAdd(DlisFrameData *frame)
{
   *m_frame_tail      = frame;

    m_frame_tail      = &(frame->next);
    m_last_frame = frame;

    DlisSet *set_file_header;

    set_file_header = m_sets;
    while (set_file_header)
    {
        if (set_file_header->next)
            set_file_header = set_file_header->next;
        else
            break;
    }
    set_file_header->frame_count++;
}

/*
* 
*/
bool CDLISParser::ObjectNameCompare(DlisValueObjName *left, DlisValueObjName *rigth)
{
    bool r;

    r = strcmp(left->identifier, rigth->identifier) == 0;
    if (r)
        r = left->origin_reference == rigth->origin_reference;
    if (r)
        r = left->copy_number == rigth->copy_number;

    return r;
}

/*
* 
*/
void CDLISParser::FlagsParserSet(UINT flag)
{
    m_state &= ~STATE_PARSER_ALL;
    m_state |= flag;
}

/*
* 
*/
char  *CDLISParser::StringTrim(char *str, size_t *len)
{
    char *begin, *end;

    begin = str;
    end   = begin + *len - 1;

    while (begin < end && *begin <= ' ')
    {
        begin++;
        (*len)--;
    }

    while (end > begin && *end <= ' ')
    {
        *end = 0;
        end--;
        (*len)--;
    }

    return begin;
}

/*
* 
*/
DlisAttribute *CDLISParser::FindColumnTemplate(DlisSet *set, DlisObject *object, DlisAttribute *attr)
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
* 
*/
DlisAttribute * CDLISParser::FindAttribute(DlisSet *set, DlisObject *object, char *name_column)
{
    DlisAttribute     *column;
    DlisAttribute     *attr;
    DlisAttribute     *r = NULL;


    column = set->colums;
    attr   = object->attr;
    while (column && attr)
    {
        if (strcmp(column->label, name_column) == 0)
        {
            r = attr;
            break;
        }

        column = column->next;
        attr   = attr->next; 
        assert(attr != NULL);
    }

    return r;
}

/*
* 
*/
DlisSet *CDLISParser::FindSubSet(char *name_sub_set, DlisSet *root)
{
    DlisSet *r = NULL, *child;

    child = root->childs; 
    while (child)
    {
        if (strcmp(child->type, name_sub_set) == 0)
        {
            r = child;
            break; 
        }
        child = child->next;
    }
    return r;
}

/*
* 
*/
DlisObject *CDLISParser::FindObject(DlisValueObjName *obj, DlisSet *set)
{
    DlisObject *r = NULL, *child;
    
    child = set->objects;
    while (child)
    {
        if (ObjectNameCompare(&child->name, obj))
        {
            r = child;
            break;
        }
        child = child->next;
    }

    return r;
}


CDLISParser::FrameData *CDLISParser::FrameDataBuild(DlisValueObjName *obj_name)
{
    DlisSet     *frame;
    DlisSet     *channel;
    FrameData   *frame_data;   


    frame_data = (FrameData *)m_allocator.MemoryGet(m_pull_id_frame_data, sizeof(FrameData));
    if (!frame_data)
        return NULL;

    memset(frame_data, 0, sizeof(FrameData));

    // get frame and channel sets 
    frame   = FindSubSet("FRAME", m_last_root_set);
    channel = FindSubSet("CHANNEL", m_last_root_set);
    
    if (!frame || !channel)
        return NULL;

    DlisObject    *obj_channel;
    DlisAttribute *attr, *found;
    
    // get object from channel set from ObjName value
    obj_channel = FindObject(obj_name, frame);
    
    if (!obj_channel)
        return NULL;
   
    // get attribute CHANNELS from
    attr = FindAttribute(frame, obj_channel, "CHANNELS");
    if (!attr)
        return NULL;



    DlisValue    *val;
    size_t        count = 0;
    ChannelInfo  *channel_info;


    channel_info = (ChannelInfo *)m_allocator.MemoryGet(m_pull_id_frame_data, sizeof(ChannelInfo) * attr->count);
    if (!channel_info)
        return NULL;

    frame_data->channel_count = 0;   
    frame_data->channels      = channel_info;

    val = attr->value;
    while (count < attr->count)
    {
        DlisValueObjName *name;

        name = (DlisValueObjName *)val->data; 
        if (!name)
            return NULL;

        channel_info->obj_name = name;

        obj_channel = FindObject(name, channel);
        if (!obj_channel)
            return NULL;
       
        found = FindAttribute(channel, obj_channel, "REPRESENTATION-CODE");
        if (!found)
            return NULL;

        channel_info->code = (RepresentationCodes) AttrGetInt(found);

        found = FindAttribute(channel, obj_channel, "DIMENSION");
        if (!found)
            return NULL;

        channel_info->dimension = (short) AttrGetInt(found);
        
        // next element 
        frame_data->channel_count++;
        channel_info++;
        val++;
        count++;
    }
    

    size_t len;
    len = strlen(obj_name->identifier);   

    frame_data->obj_key.copy_number      = obj_name->copy_number;
    frame_data->obj_key.origin_reference = obj_name->origin_reference;
    frame_data->obj_key.identifier       = m_allocator.MemoryGet(m_pull_id_frame_data, len + 1);
    strcpy_s(frame_data->obj_key.identifier, len + 1, obj_name->identifier);



    FrameData **frame_tail;

    frame_tail = &m_frame_data;
    while (*frame_tail)
    {
        frame_tail = &(*frame_tail)->next;
    }
    *frame_tail = frame_data;

    return frame_data; 
}


bool CDLISParser::FrameDataParse(FrameData *frame)
{
    static char   buf[8 * Kb];

    void         *dst;
    size_t        len          = 0;
    int           number_frame = 0;

    int           count;
    ChannelInfo  *channel;

    while(m_segment.len)
    {
        ReadCodeSimple(RC_UVARI, &dst, &len);
        memcpy(&number_frame, dst, len);

        channel  = frame->channels;
        count    = frame->channel_count;

        for (int i = 0; i < count; i++)
        {
            // ReadRawData(buf, channel->dimension * 8);
             char *ptr;

             ptr = &buf[0];
             for (int j = 0; j < channel->dimension; j++)
             {
                 ReadCodeSimple(channel->code, &dst, &len);
                 memcpy(ptr, dst, len);
                 ptr      += len;
             }

             float *val;

             val = (float *)buf;

            channel++;
        }
        // count_frame--;
    }
    return true;
}


CDLISParser::FrameData *CDLISParser::FrameDataFind(DlisValueObjName *obj_name)
{
    FrameData *next, *r = NULL;

    next = m_frame_data;
    while (next)
    {
        if (ObjectNameCompare(&next->obj_key, obj_name))
        {
            r = next;
            break;
        }

        next = next->next;
    }

    return r;
}

/*
*  читаем Set
*/
bool CDLISParser::ReadSet()
{
    char      *val;
    size_t     len;
    DlisSet   *set;
   

    FlagsParserSet(STATE_PARSER_SET);

    set = (DlisSet *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisSet));
    if (!set)
        return false;

    memset(set, 0, sizeof(DlisSet));

    set->type_set = m_segment_header.type;

    if (m_component_header.format & TypeSet::TypeSetType)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);
        
        set->type = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        if (!set->type)
            return false;
        
        strcpy_s(set->type, len + 1, val);
    }

    if (m_component_header.format & TypeSet::TypeSetName)
    {
        ReadCodeSimple(RC_IDENT, (void **)&val, &len);

        set->name = m_allocator.MemoryGet(m_pull_id_strings, len + 1);
        if (!set->name)
            return false;
        
        strcpy_s(set->name, len + 1, val);
    }

    SetAdd(set);

    return true;
}

/*
* читаем объект
*/
bool CDLISParser::ReadObject()
{
	DlisObject  *obj;

    FlagsParserSet(STATE_PARSER_OBJECT);
    
    obj = (DlisObject *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisObject));
    memset(obj, 0, sizeof(DlisObject));

    if (m_component_header.format & TypeObject::TypeObjectName)
    {
        ReadCodeComplex(RC_OBNAME, (void **)&obj->name);
    }

    ObjectAdd(obj);
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
    RepresentationCodes rep_code = RC_IDENT;

    // если первый атрибут, определим это атрибут объекта или шаблона
    if (m_state & STATE_PARSER_SET)
    {
        FlagsParserSet(STATE_PARSER_TEMPLATE_ATTRIBUTE);
    }
    if (m_state & STATE_PARSER_OBJECT)
    {
        FlagsParserSet(STATE_PARSER_ATTRIBUTE);
    }


    DlisAttribute *attr;
    
    attr = (DlisAttribute *)m_allocator.MemoryGet(m_pull_id_objects, sizeof(DlisAttribute));
    memset(attr, 0, sizeof(DlisAttribute));
    attr->count = 1;

    if (m_state & STATE_PARSER_TEMPLATE_ATTRIBUTE)
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
        if (m_state & STATE_PARSER_ATTRIBUTE)
        {
            DlisAttribute *column;

            column = FindColumnTemplate(m_last_set, m_last_object, attr);
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
/*
* принтуем код объекта в его строковое описание (для отладки)
*/
void CDLISParser::DebugPrintRepCode(RepresentationCodes code, char *str_rep_code, size_t size)
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
