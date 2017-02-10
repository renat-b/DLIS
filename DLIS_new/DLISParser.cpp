#include "DLISParser.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"


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


CDLISParser::RepresentaionCodesLenght CDLISParser::s_rep_codes_lenght[] = 
{
    { RC_FSHORT, 2  },
    { RC_FSINGL, 4  },
    { RC_FSING1, 8  },
    { RC_FSING2, 12 },
    { RC_ISINGL, 4  },
    { RC_VSINGL, 4  },
    { RC_FDOUBL, 8  },
    { RC_FDOUB1, 16 },
    { RC_FDOUB2, 24 },
    { RC_CSINGL, 8  },
    { RC_CDOUBL, 16 },
    { RC_SSHORT, 1  },
    { RC_SNORM,  2  },
    { RC_SLONG,  4  },
    { RC_USHORT, 1  },
    { RC_UNORM,  2  },
    { RC_ULONG,  4  },
    { RC_UVARI,  1, },
    { RC_DTIME,  8  },
    { RC_STATUS, 1  },
};


CDLISParser::CDLISParser() : m_file(INVALID_HANDLE_VALUE)
{
    memset(&m_storage_unit_label, 0, sizeof(m_storage_unit_label));
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));
}


CDLISParser::~CDLISParser()
{

}


bool CDLISParser::Parse(const char *file_name)
{
    if (!file_name)
        return false;

    if (!FileOpen(file_name))
        return false;
    
    if (!ChunkInitialize())
        return false;
        
    if (!ReadStorageUnitLabel())
        return false;
    
    if (!ReadLogicalFiles())
        return false;

    return true;
}


bool CDLISParser::Initialize()
{
   memset(&m_file_chunk, 0, sizeof(m_file_chunk)); 
   return true;
}


void CDLISParser::Shutdown()
{
    FileClose();

    m_file_chunk.Free();
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));
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


void CDLISParser::Big2LittelEndian(void *data, int len)
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


void CDLISParser::RoleAndFormatFromByte(byte bt, byte *role, byte *format)
{
    Big2LittelEndianByte(&bt);
    
    *role   = bt & 0x7;
    *format = bt & 0xF8;
}


bool CDLISParser::ChunkNextBuffer(char **data, size_t len)
{

    if (len <= m_file_chunk.remaind)      
    {
        *data = m_file_chunk.data + m_file_chunk.pos;

        m_file_chunk.pos       += len;
        m_file_chunk.remaind   -= len;
        return true;
    }


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


    DWORD   amout;
    
    amout = min(FILE_CHUNK, (DWORD)(m_file_chunk.file_remaind));    

    if (!m_file_chunk.Resize(amout + m_file_chunk.remaind))
        return false;
 

    if (!FileRead(m_file_chunk.data + m_file_chunk.remaind, amout))
        return false; 


    m_file_chunk.file_remaind -= amout;
    m_file_chunk.remaind      += amout;
    m_file_chunk.pos           = 0;
    m_file_chunk.size_chunk    = m_file_chunk.remaind;
    
    if (len < m_file_chunk.remaind)      
    {
        *data = m_file_chunk.data;

        m_file_chunk.pos      += len;
        m_file_chunk.remaind  -= len;
        return true;
    }
       
    return false;
}


bool CDLISParser::ChunkInitialize()
{
    m_file_chunk.Free();
    memset(&m_file_chunk, 0, sizeof(m_file_chunk));
    
    m_file_chunk.file_remaind = FileSize();
    return true;
}


bool CDLISParser::ChunkEOF()
{
    return (m_file_chunk.remaind == 0 && m_file_chunk.file_remaind == 0);
}


bool CDLISParser::SegmentIsSet(byte role)
{
    bool r;
    
    r = (role == TypeRole::Redundant_Set) || (role == TypeRole::Replacement_Set) || (role == TypeRole::Set); 
    return r;
}


bool CDLISParser::SegmentIsObject(byte role)
{
    bool r;
    
    r = role == TypeRole::Object;
    return r;
}


bool CDLISParser::SegmentIsAttr(byte role)
{
    bool r;
    
    r = (role == TypeRole::Attribute) || (role == TypeRole::Invariant_Attribute) || (role == TypeRole::Absent_Attribute); 
    return r;
}


bool CDLISParser::VisibleRecordNext(char **record, size_t *len)
{
    *record = NULL;
    *len    = 0;

    char              *data;
    LogicalFileHeader *header;

    bool r = ChunkNextBuffer(&data, sizeof(LogicalFileHeader));

    if (r) 
    {
        header = (LogicalFileHeader *)data;
        Big2LittelEndian(&((*header).length), sizeof(header->length));
    }
    
    if (r)
    {
        *len = header->length - sizeof(LogicalFileHeader);
        r = ChunkNextBuffer(&data, *len);            
    }

    if (r)
        *record = data;

    return r;
}


bool CDLISParser::ReadStorageUnitLabel()
{
    //!!! FileRead((char *)&m_storage_unit_label, sizeof(m_storage_unit_label));
    //return true;

    bool    r;
    char   *data;

    r = ChunkNextBuffer(&data, sizeof(m_storage_unit_label));
    
    if (r)
        m_storage_unit_label = *(StorageUnitLabel *)data;

    return r;
}


bool CDLISParser::ReadLogicalFiles2()
{
    size_t   size = 80;

    LogicalFileHeader header = { 0 };

    char *data = (char *)&header;
    DWORD len  = sizeof(header);
    
    UINT64  file_size;
    UINT64  file_seek;

    file_size = FileSize();

    bool r = FileRead(data, len);
    size += len;

    if (!r)
        return false;

    Big2LittelEndian(&header.length, sizeof(header.length));
    header.length -= sizeof(header);

    if (header.length)
    {
       do 
       {
            //r = ReadLogicalFile();
            char  *buf;
            
            buf = new(std::nothrow) char[header.length];
            if (!buf)
                return false;
            
            r = FileRead(buf, header.length);
            size += header.length;
            delete buf;

            printf("data len: %d\n", header.length);

            if (r)
            {
                file_seek = FileSeekGet();
                if (file_seek >= file_size)
                    break;

                data = (char *)&header;
                len  = sizeof(header);

                r = FileRead(data, len);
                size += len;
                if (!r)
                    return false;

                Big2LittelEndian(&header.length, sizeof(header.length));
                header.length -= sizeof(header);
            }

            if (!r)
                break;
       } 
       while (header.length);
    }

    return r;
}


bool CDLISParser::ReadLogicalFiles()
{
    size_t  len;
    char   *record;

    static int count = 0;

    bool  r = VisibleRecordNext(&record, &len); 
    if (r)
    {
        do
        {
            if (ChunkEOF())
                break;

            count ++;

            r = ReadSegments(record, len); 
            if (r)
                r = VisibleRecordNext(&record, &len);
        }
        while (r);
    }

    return r;
}


bool CDLISParser::ReadLogicalFile()
{
    while (true)
    {
        ReadLogicalRecord();
    }    
}


bool CDLISParser::ReadSegments(char *record, size_t len)
{
    SegmentHeader *header;
    char          *logical_record;

    logical_record = record;
    while (logical_record < (record + len))
    {
        header = (SegmentHeader *)logical_record;
        Big2LittelEndian(&(*header).length, sizeof(header->length));

        ReadSegment(logical_record + sizeof(SegmentHeader), header);

        logical_record += header->length;        
    }

    assert(logical_record == (record + len));

    return true;
}


bool CDLISParser::ReadSegment(char *logical_record, SegmentHeader *header)
{
    (void)header;
    (void)logical_record;

    char *segment = logical_record;
    byte  role;
    byte  format; 

    int  k;   //!!!

    while (segment < (logical_record + header->length))
    {
        RoleAndFormatFromByte((byte)*segment, &role, &format);

        segment ++;

        if (SegmentIsSet(role))
        {
           k = 0;
        }
        else if (SegmentIsObject(role))
        {
            k = 0; 
        }
        else if (SegmentIsAttr(role))
        {
            k = 0;
        }

    }
      
    return true;
}

bool CDLISParser::ReadLogicalRecord()
{
    SegmentHeader   header = { 0 };

    char    *data = (char *)&header;
    DWORD    len  = sizeof(header);

    bool     r;

    r = FileRead(data, len);
    if (!r)
        return false;
    
    DWORD trailing_len = 0;
    DWORD body_len     = header.length - sizeof(header); 

    data = new(std::nothrow) char[body_len];
    
    if (!data)
        return false;
    
    r = FileRead(data, header.length);
    if (!r)
    {
        delete data;
        return false;
    }
        
    if (header.attributes & LogicalRecordSegmentAttributes::Trailing_Length)
    {
        trailing_len += 2;
        body_len     -= 2;
    }

    if (header.attributes & LogicalRecordSegmentAttributes::Checksum)
    {
        trailing_len += 2;
        body_len     -= 2;
    }

    if (header.attributes & LogicalRecordSegmentAttributes::Padding)
    {
        byte   pad  = 0;
        UINT64 seek = FileSeekGet();
        
        
    }

    delete data;
    data = NULL;
    
    return true;
}


bool CDLISParser::ReadLogicalRecordHeader()
{
    return false;
}


bool CDLISParser::ReadLogicalRecordBody()
{
    return false;
}


bool CDLISParser::ReadRawData(void *dst, const char **src, size_t len)
{
    memcpy(dst, *src, len);
    *src = *src + len;

    return true;
}


bool CDLISParser::ReadString(char *dst, const char **src)
{
    size_t        size;
    const char   *p = *src;

    size = *p;
    p++;
    
    memcpy(dst, p, size);
    *src = *src + size; 

    return true;
}


bool CDLISParser::ReadRepresentationCode(const RepresentaionCodes code, void *dst, const char **src)
{
    char tmp[128] = { 0 };

    switch (code)
    {
        case RC_FSHORT:
        case RC_FSINGL:
        case RC_FSING1:
        case RC_FSING2:
        case RC_ISINGL:
        case RC_VSINGL:
        case RC_FDOUBL:
        case RC_FDOUB1:
        case RC_FDOUB2:
        case RC_CSINGL:
        case RC_CDOUBL:
        case RC_SSHORT:
        case RC_SNORM: 
        case RC_SLONG: 
        case RC_USHORT:
        case RC_UNORM: 
        case RC_ULONG: 
        case RC_UVARI: 
        case RC_DTIME: 
        case RC_STATUS:
            {
            	for (int i = 0; i < _countof(s_rep_codes_lenght); i++)
            	    if (s_rep_codes_lenght[i].code == code)
            	    {
            	        ReadRawData(dst, src, s_rep_codes_lenght[i].lenght);
            		    break;
            	    }

                break;
            }

        case RC_IDENT:
        case RC_ASCII:
        case RC_OBNAME:    
            ReadString((char *)dst, src); 
            break;

        default:
            assert(false);
            break;
    }

    return true;
}


bool CDLISParser::ReadCharacteristics(byte format, const char **src)
{
    size_t              val;
    char                str[128];
    RepresentaionCodes  rep_code;
    
    if (format & TypeAttribute::TypeAttrLable)
        ReadRepresentationCode(RC_IDENT, &val, src);

    if (format & TypeAttribute::TypeAttrCount)
        ReadRepresentationCode(RC_UVARI, &val, src);

    if (format & TypeAttribute::TypeAttrRepresentationCode)
        ReadRepresentationCode(RC_USHORT, &rep_code, src);

    if (format & TypeAttribute::TypeAttrUnits)
        ReadRepresentationCode(RC_IDENT, str, src);

    if (format & TypeAttribute::TypeAttrValue)
        ReadRepresentationCode(rep_code, str, src);
    
    return true;
}


bool CDLISParser::ReadObject(byte format, const char **src)
{
    char str[128];

    if (format & TypeObject::TypeObjectName)
        ReadRepresentationCode(RC_IDENT, str, src);

    return true;
}


bool CDLISParser::ReadSet(byte format, const char **src)
{
    char tmp[128];

    if (format & TypeSet::TypeSetType)
        ReadRepresentationCode(RC_IDENT, tmp, src);

    if (format & TypeSet::TypeSetName)
        ReadRepresentationCode(RC_IDENT, tmp, src);

    return true;
}