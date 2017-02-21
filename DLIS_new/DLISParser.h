#pragma once 

#include    "new.h"
#include    "windows.h"
#include    "FileBin.h"

extern CFileBin *g_global_log;

#pragma pack(push, 1)
struct StorageUnitLabel
{
    char        storage_unit_sequence_number[4];
    char        dlis_version[5];
    char        storage_unit_structure[6];
    char        maximum_record_length[5];
    char        storage_set_identifier[60];
};
#pragma pack(pop)


#pragma pack(push, 1)
struct  VisibleRecordHeader
{
    short   length;
    byte    frm[2];
};
#pragma pack(pop)


#pragma pack(push, 1)
struct  SegmentHeader
{
    short              length;                 // segment length
    unsigned char      attributes;             // segment attributes byte
    unsigned char      type;                   // logical record type byte

    short              length_data;            // размер данных за минусом метаданных
};
#pragma pack(pop)


#pragma pack(push, 1)
struct ComponentHeader
{
    unsigned char  role;
    unsigned char  format;
};
#pragma pack(pop)


// флаги Role (верхние три бита)
enum TypeRole
{
    // атрибут
    Absent_Attribute    = 0x0,     //  000     ABSATR     Absent Attribute
    Attribute           = 0x1,     //  001     ATTRIB     Attribute
    Invariant_Attribute = 0x2,     //  010     INVATR     Invariant Attribute
    // объект
    Object              = 0x3,     //  011     OBJECT     Object

    //100 	reserved -

    // set
    Redundant_Set       = 0x5,     //  101 	   RDSET 	  Redundant Set
    Replacement_Set     = 0x6,     //  110 	   RSET 	  Replacement Set
    Set                 = 0x7,     //  111 	   SET 	      Set
};


enum TypeSet
{
    TypeSetType         = 0x08,
    TypeSetName         = 0x10,
};


enum TypeObject
{
    TypeObjectName      = 0x08,
};


enum TypeAttribute
{
    TypeAttrLable       = 0x08,
    TypeAttrCount       = 0x10,
    TypeAttrRepresentationCode = 0x20,
    TypeAttrUnits       = 0x40,
    TypeAttrValue       = 0x80,
};


enum LogicalRecordSegmentAttributes
{
    Logical_Record_Structure = 0x01,        // 0 = Indirectly Formatted Logical Record, 1 = Explicitly Formatted Logical Record
    Predecessor              = 0x02,        // 0 = This is the first segment of the Logical Record, 1 = This is not the first segment of the Logical Record
    Successor                = 0x04,        // 0 = This is the last Segment of the Logical Record, 1 = This is not the last Segment of the Logical Record
    Encryption               = 0x08,        // 0 = No encryption,  1 = Logical Record is encrypted
    Encryption_Packet        = 0x10,        // 0 = No Logical Record Segment Encryption Packet, 1 = Logical Record Segment Encryption Packet is present
    Checksum                 = 0x20,        // 0 = No checksum, 1 = A checksum is present in the LRST
    Trailing_Length          = 0x40,        // 0 = No Trailing Length,  1 = A copy of the LRS lengt is present in the LRST
    Padding                  = 0x80,        // 0 = No record padding,  1 = Pad bytes are present in LRST
};


enum RepresentaionCodes
{
    RC_FSHORT = 1,     // 2 	Low precision floating point
    RC_FSINGL = 2,     // 4 	IEEE single precision floating point
    RC_FSING1 = 3,     // 8 	Validated single precision floating point
    RC_FSING2 = 4,     // 12 	Two - way validated single precision floating point
    RC_ISINGL = 5,     // 4 	IBM single precision floating point
    RC_VSINGL = 6,     // 4 	VAX single precision floating point
    RC_FDOUBL = 7,     // 8 	IEEE double precision floating point
    RC_FDOUB1 = 8,     // 16 	Validated double precision floating point
    RC_FDOUB2 = 9,     // 24 	Two - way validated double precision floating point
    RC_CSINGL = 10,    // 8 	Single precision complex
    RC_CDOUBL = 11,    // 16 	Double precision complex
    RC_SSHORT = 12,    // 1 	Short signed integer
    RC_SNORM  = 13,    // 2 	Normal signed integer
    RC_SLONG  = 14,    // 4 	Long signed integer
    RC_USHORT = 15,    // 1 	Short unsigned integer
    RC_UNORM  = 16,    // 2 	Normal unsigned integer
    RC_ULONG  = 17,    // 4 	Long unsigned integer
    RC_UVARI  = 18,    // 1, 2, or 4 	Variable - length unsigned integer
    RC_IDENT  = 19,    // V 	Variable - length identifier
    RC_ASCII  = 20,    // V 	Variable - length ASCII character string
    RC_DTIME  = 21,    // 8 	Date and time
    RC_ORIGIN = 22,    // V 	Origin reference
    RC_OBNAME = 23,    // V 	Object name
    RC_OBJREF = 24,    // V 	Object reference
    RC_ATTREF = 25,    // V 	Attribute reference
    RC_STATUS = 26,    // 1 	Boolean status
    RC_UNITS  = 27,    // 
    RC_LAST   = RC_UNITS,
};


// Типы Explicitly Formatted Logical Record
enum EFLRType
{
    FHLR    = 0,        // File Header
    OLR     = 1,        // Origin
    AXIS    = 2,        // Coordinate Axis
    CHANNL  = 3,        // Channel-related information
    FRAME   = 4,        // Frame Data
    STATIC  = 5,        // Static Data
    SCRIPT  = 6,        // Textual Data
    UPDATE  = 7, 
    UDI     = 8,
    LNAME   = 9,
    SPEC    = 10,
    DICT    = 11,
    LAST_Public_EFLR_Code = DICT,
};

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
    //
    HANDLE              m_file;
    StorageUnitLabel    m_storage_unit_label;
    
    struct FileChunk : MemoryBuffer
    {
        size_t      pos;
        size_t      remaind;
        size_t      size_chunk;
        UINT64      file_remaind;
    };
    
    FileChunk        m_file_chunk;

    struct VisibleRecord
    {
        char      *current;
        char      *end; 
        size_t     len;
    };

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

private:
   static RepresentaionCodesLenght s_rep_codes_length[RC_LAST];
 
public:
    CDLISParser();
    ~CDLISParser();

    bool            Parse(const char *file_name);

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

    void            Big2LittelEndian(void *dst, size_t len);
    void            Big2LittelEndianByte(byte *byte);
     
    bool            ChunkNextBuffer(char **data, size_t len);
    bool            ChunkInitialize();
    bool            ChunkEOF();

    bool            SegmentGet();
    bool            SegmentProcess(); 

    bool            VisibleRecordNext();
    bool            StorageUnitLabelRead();
    bool            ReadLogicalFiles();
    bool            ReadComponent();

    bool            HeaderSegmentGet(SegmentHeader *header);
    bool            HeaderComponentGet();

    bool            ReadRawData(void *dst, size_t len);

    bool            ReadRepresentationCode(RepresentaionCodes code, void **dst, size_t *len, int count = 1);

    bool            ReadSet();
    bool            ReadObject();
    bool            ReadAttribute();

    void            DebugPrintRepCode(RepresentaionCodes code, char *str_rep_code, size_t size);
};