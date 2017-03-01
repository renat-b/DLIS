#pragma once 

#include    "new.h"
#include    "DlisCommon.h"
#include    "FileBin.h"
#include    "DlisAllocator.h"

// глобальный объект лога (для отладки)
extern CFileBin *g_global_log;


// буфер реаллокации данных
struct MemoryBuffer
{
    char            *data;
    size_t           size;
    size_t           max_size;

    bool             Resize(size_t new_len); 
    void             Free();    
};


class CDLISParser
{
private:
    enum constants
    {
        Kb         = 1024,
        Mb         = Kb * Kb,
        FILE_CHUNK = 16 * Mb,

        MAX_TEMPLATE_ATTRIBUTES   = 32,
        //
        STATE_PARSER_FIRST        = 0x00,
        STATE_PARSER_SET          = 0x01,
        STATE_PARSER_OBJECT       = 0x02,
        STATE_PARSER_TEMPLATE_ATTRIBUTE = 0x04,
        STATE_PARSER_ATTRIBUTE    = 0x08, 
    };
    
    struct TemplateAttributes
    {
        RepresentaionCodes  code;
        UINT                count;
    };

    struct RepresentaionCodesLenght
    {
        RepresentaionCodes  code;
        int                 length;
    };

    typedef unsigned char byte;
    //  хендл файла 
    HANDLE              m_file;
    // заголовок DLIS
    StorageUnitLabel    m_storage_unit_label;

    // внутренний буфера чтения из файла DLIS    
    struct FileChunk : MemoryBuffer
    {
        size_t      pos;
        size_t      remaind;
        size_t      size_chunk;
        UINT64      file_remaind;
    };
    
    FileChunk        m_file_chunk;

    // структура DLIS visible record
    struct VisibleRecord
    {
        char      *current;
        char      *end; 
        size_t     len;
    };

    // структура DLIS сегмента 
    struct SegmentRecord
    {
        char            *current;
        char            *end;
        size_t           len;
    };
    

    VisibleRecord      m_visible_record;
    SegmentRecord      m_segment;

    SegmentHeader      m_segment_header;
    ComponentHeader    m_component_header;


    UINT               m_state;

    TemplateAttributes m_template_attributes[MAX_TEMPLATE_ATTRIBUTES];
    UINT               m_template_attributes_count;
    UINT               m_attributes_count;
    UINT               m_object_num;

    DlisSet           *m_sets;
    DlisSet           *m_set_current;
    DlisObject        *m_object_current;


    CDLISAllocator     m_allocator;
    size_t             m_pull_strings;
    size_t             m_pull_objects;
    


private:
   static RepresentaionCodesLenght s_rep_codes_length[RC_LAST];
 
public:
    CDLISParser();
    ~CDLISParser();

    // парсинг DLIS
    bool            Parse(const char *file_name);

    // инициализация, выгрузка внутренних буферов и данных из парсера
    bool            Initialize();
    void            Shutdown();

private:
    //  чтение файла
    bool            FileOpen(const char *file_name);
    bool            FileClose();
    bool            FileRead(char *data, DWORD len);
    bool            FileWrite(const char *data, DWORD len);
    UINT64          FileSeekGet();
    void            FileSeekSet(UINT64 offset);
    UINT64          FileSize();
    // преобразования bit 2 littel endian
    void            Big2LittelEndian(void *dst, size_t len);
    void            Big2LittelEndianByte(byte *byte);

    // чтение заголовка и логических файлов
    bool            ReadStorageUnitLabel();
    bool            ReadLogicalFiles();

    // чтение внутреннего буфера
    bool            BufferNext(char **data, size_t len);
    bool            BufferInitialize();
    bool            BufferIsEOF();
    bool            VisibleRecordNext();

    // чтение сегмента DLIS данных 
    bool            SegmentGet();
    bool            SegmentProcess(); 

    // чтение компонента DLIS
    bool            ComponentRead();
    bool            ComponentHeaderGet();

    // чтение сырых данных DLIS
    bool            ReadRawData(void *dst, size_t len);
    bool            ReadRepresentationCode(RepresentaionCodes code, void **dst, size_t *len, int count = 1);

    // чтение атрибутов компонента DLIS
    bool            ReadSet();
    bool            ReadObject();
    bool            ReadAttribute();


    void            SetAdd(DlisSet *set);
    void            ObjectAdd(DlisObject *obj);
    // распечатка code representation
    void            DebugPrintRepCode(RepresentaionCodes code, char *str_rep_code, size_t size);
    void            DebugPrintAttrCode(UINT attr_code, char *str_attr_code, size_t size);
};