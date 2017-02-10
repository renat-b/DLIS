/*! \file */

/*! \mainpage
	Модуль поддержки чтения и записи данных в формате DLIS 1.0 \n\n
	Автор: Александр Газизулин
			(<a href="http://tgtoil.com"> TGT Oil & Gas Services</a>) \n
	Язык: C++03 \n
	Используемые библиотеки: STL \n
	Версия: 0.1 \n
	Дата: 31.07.2015 \n\n
	Описание формата DLIS 1.0 см. в документе
		<a href="http://w3.energistics.org/RP66/V1/Toc/main.html">
		RP66 V1</a>. */
// Doxy: Чтобы скобки вокруг первой URL-ссылки не отделялись пробелами, они
//		 должны быть на той же строке

// Образец URL для Doxygen:
/*
		<a href="URL">
		LINKTEXT</a>
*/

//	Секция интерфейса

#ifndef DLIS
#define DLIS

#include <string>
#include <vector>
#include <list>
#include <map>
#include <valarray>
#include <sstream>
#include <iostream>
#include <fstream>

#include "dlis_iter.h"

namespace Dlis {

using std::string;
using std::vector;

typedef unsigned char byte;		//!< 8-разрядное целое без знака
typedef signed char   int8;		//!< 8-разрядное целое со знаком

/*! \name Platform-dependent definitions
	Определения, зависящие от реализации. \n
	При необходимости следует заменить значение littleEndian и типы,
	для которых введены алиасы,	на подходящие типы, а затем перекомпилировать
	модуль. */
///@{
const bool littleEndian = true;
//!< Порядок байт. Зависит от реализации: например, true для Intel или VAX и false для Motorola или IBM
// - NB: Многострочные brief-описания глобальных элементов не работают
typedef unsigned short uint16;	//!< 16-разрядное целое без знака
typedef short int16;			//!< 16-разрядное целое со знаком
typedef unsigned int uint32;	//!< 32-разрядное целое без знака
typedef int int32;				//!< 32-разрядное целое со знаком
typedef float ieeeSingle;		//!< Число одинарной точности в формате IEEE 754
typedef double ieeeDouble;		//!< Число двойной точности в формате IEEE 754
// doxy kludge (чтобы Doxygen не терял предыдущее brief-описание)
///@}
// - NB: При detailed-описаниях глобальных элементов Doxygen генерирует
//		 неправильный HTML-код
// - NB: Можно ввести программный контроль годности выбранных типов

//! Сведения о степени успешности выполнения операции (Return Info).
/*! Большинство функций заносят значение этого типа либо в возвращаемое
	значение, либо в дополнительный параметр */
class RI {
public:
	//! Коды возврата (использовать не рекомендуется).
	/*!	В будущих версиях они, возможно, будут заменены в классе RI на
		безымянные целые числа (а само определение типа будет перенесено
		в секцию реализации) */
	enum RetCode {
		Ok,
	// Группа вспомогательных кодов
		RdSet,
//	// Группа кодов предупреждения
			MinWarn = 100,
		NoVal, NonFrameData,
		BadFrameNum, /*NoFrames,*/
		NoLFs, NonStdLRSH, NonStdSet, RepSet, NonStdFHLR, NonStdFH,

		LFWritten, DupObj, DupAttr,

		/*SULIdTrunc, FHNumFixed,*/ FHIdTrunc,
		FromRawPrec, WrongAttr, ManyObjAttrs, AmbigObjs,
		ToRawPrec,
		BadObj, ShortFrame,
			MaxWarn,
//	// Группа кодов критических ошибок
//			MinError = 500,
		BadChan, BadFrameObj, NoChan, BadFrame, NoFrame,
		/*BadValIndex,*/ BigValIndex, NonContig,
		BadRaw, FromRawNone, FromRawValErr, FromRawTypeErr,

		ToRawNone, ToRawValErr, ToRawTypeErr,

		AmbigChans,
		BadHdr, BadVRH, BadLRSH, BadLR, BadFHLRPos, BadSet, NoSetType, BadTpl,
		NoObjName, BadAttr, BadCompDescr, BadFH,

		NonStdAttr, BadRepr, BadUnits, BadDTime, BadDim, WrongCount, WrongCode,
		WrongValType,
		SULLongId, FHBadNum, BadDefOriId, NoUserFH, BadObjType, BadObjName,
		BadAttrLabel,
		AddObjErr, /*BadOriId,*/ LFNotWritten, NoFrameType, NotAllChans,

		ReadErr, WriteErr, OpenErr, DataTrunc,
		OutOfMem,
		Platform,
		ProgErr,
			MaxError,
		LastRC
	};
	// - Коды считаются упорядоченными по степени серьезности ошибки
	//	 (хотя внутри групп упорядоченность не имеет смыслового значения)
/* - NB: Можно ввести статический член policy, в котором указывать,
		 нужно ли аварийно завершать операцию при некритической ошибке
		 (это особенно полезно при отладке, т.к. некритические ошибки
		 на реальных DLIS-файлах, скорее всего, говорят об ошибках программы */
	RI() :
		m_rc(Ok), m_rcc(0), m_crit(false) {}
//! \cond
	RI(RetCode retCode, unsigned rcCase = 0) :
		m_rc(retCode), m_rcc(rcCase), m_crit(false) {}
//! \endcond
	static const string &message(RetCode retCode) {
		return msgTable().message(retCode);
	}
	//!< Сообщение об ошибке для кода retCode
//! \cond
	friend bool operator<(const RI &ri1, const RI &ri2);
//! \endcond
	RetCode retCode() const { return m_rc; }
	//!< Код возврата
	string getMessage() const;
	//!< Возвращает уточненное сообщение об ошибке.
	/*!< Возвращает сообщение об ошибке с уточняющим кодом (Case), позволяющим
		 однозначно определить место ошибки в программе */
	bool ok() const { return m_rc < MinWarn ; }
	//!< Возвращает true при отсутствии ошибки
//	operator bool() const { return ok(); }
// - NB: Практика показала, что это потенциально опасная функция
	bool critical() const { return m_crit; }
	//!< Признак критической ошибки.
	/*!< Свидельствует о том, что из-за этой ошибки выполнение операции было
		 прекращено */
	void clear() {
		m_rc = Ok;
		m_rcc = 0;
		m_crit = false;
	}
//! \cond
	void setOk() { clear(); }
	RI &toCritical() { m_crit = true; return *this; }
	RI &upTo(const RI &ri);
//! \endcond
private:
	struct MsgTable : std::map<RetCode, string> {
		MsgTable() {
			add(Ok,			"Ok");
			add(NoVal,		"Absent Value");
			add(NonFrameData,
				"Non-FDATA IFLR not supported");
//			add(BadValIndex,
//				"Illegal value index");
			add(BigValIndex,
				"Specified value index exceeds count characteristics");
			add(NonContig,	"Non-contiguous source/destination container");
			add(AmbigChans,	"Duplicate Channel Object names");
			add(BadHdr,		"Invalid DLIS header");
			add(BadVRH,		"Invalid Visible Record Header");
			add(BadLRSH,	"Invalid Logical Record Segment Header");
			add(BadLR,		"Invalid Logical Record format");
			add(BadFHLRPos,
				"File Header Logical Record not found or found at wrong place");
			add(BadSet,		"Invalid Set format");
			add(NoSetType,	"Null Set Type identifier");
			add(BadTpl,		"Invalid Set Template");
			add(NoObjName,	"Null Object name identifier");
			add(BadAttr,	"Invalid Object Attribute");
			add(BadCompDescr,
				"Invalid Component Descriptor");
			add(BadFH,		"None or invalid File Header");
			add(BadRaw,		"Invalid DLIS value format");
			add(FromRawNone,
				"DLIS format cannot be converted to this type");
			add(FromRawValErr,
				"Actual DLIS value cannot be loaded into this type");
			add(FromRawTypeErr,
				"This DLIS format cannot be generally loaded into this type");
			add(ToRawNone,
				"Type cannot be converted to this DLIS format");
			add(ToRawValErr,
				"Actual value cannot be saved in this format");
			add(ToRawTypeErr,
				"Values of this type cannot be generally saved in this format");
//			add(NoFrames,	"No frames of this type found");
			add(BadFrameNum,
				"Invalid Frame number");
			add(NoLFs,		"No Logical Files found");
			add(DupAttr,	"Attribute already exists");
			add(DupObj,		"Object already exists");
			add(LFWritten,	"Current Logical File already written");
			add(NonStdLRSH, "Non-standard Logical Record Segment Header");
			add(NonStdSet,	"Non-standard Set structure");
			add(RepSet,		"Replacement Set not supported");
			add(NonStdFHLR,
				"Non-standard File Header Logical Record structure");
			add(NonStdFH,	"Non-standard File Header Object");
			add(BadUnits,	"Non-standard units specified");
			add(BadDTime,	"Invalid DateTime value format");
			add(BadDim,		"Invalid Dimension specified");
			add(WrongCount,
				"Channel value count does not agree with DIMENSION value");
			add(WrongValType,
				"Suitable Representation Code for this value not found");
			add(BadRepr,	"Invalid Representation Code specified");
			add(SULLongId,	"Too long Storage Set Identifier");
			add(FHBadNum,	"Wrong Logical File SEQUENCE-NUMBER");
			add(FHIdTrunc,	"Too long Logical File ID - truncated");
			add(FromRawPrec,
				"Loss of precision while conversion from DLIS format");
			add(ToRawPrec,
				"Loss of precision while conversion to DLIS format");
			add(WrongAttr,	"Invalid Object Attribute skipped");
			add(ManyObjAttrs,
				"Out-of-template Object Attributes discarded");
			add(AmbigObjs,	"Duplicate Object Names");
			add(BadObj,		"Invalid Object discarded");
			add(ShortFrame, "Unused bytes left in Frame Data IFLR");
			add(NoChan,		"Frame Object with lacking channel discarded");
			add(BadChan,
				"Channel Object with invalid main attributes discarded");
			add(BadFrameObj,
				"Frame Object with invalid channel references discarded");
			add(BadFrame,	"Frame with lacking Frame Object discarded");
			add(NoFrame,	"Frame with specified number not found");
			add(ProgErr,	"Program error");
			add(Platform,	"Compiler or platform incompatibility");
//			add(BadOriId,	"Invalid Origin Id parameter");
			add(LFNotWritten,
							"Current Logical File metadata is not yet written");
			add(NoFrameType,
							"Frame Type not found in current Logical File");
			add(NotAllChans,
							"Not all channels found for this Frame Object");
			add(BadDefOriId,
				"Too big Defining Origin Id");
			add(NonStdAttr, "Non-standard attribute");
			add(BadAttrLabel,
				"Invalid Attribute Label");
			add(NoUserFH,	"File Header object cannot be created directly");
			add(BadObjType, "Uncertain Object Type");
			add(BadObjName, "Invalid Object Name");
			add(AddObjErr,	"Could not add new object");
			add(ReadErr,	"Read error");
			add(WriteErr,	"Write error");
			add(OpenErr,	"File open error");
			add(DataTrunc,	"Unexpected end of data");
			add(OutOfMem,	"Out of memory");
// - NB: Надо бы добавить проверку единственности вызова add для каждого кода
//		 и функцию выдачи тех кодов (хотя бы их количество), которым
//		 не назначены строки сообщений
		}
		const string &message(RetCode retCode) const {
			const_iterator it = find(retCode);
			return it != end() ? it->second : strEmpty;
		}
		void add(RetCode rc, const string s)	// - для удобства
			{ insert(std::make_pair(rc, s)); }
		string strEmpty;						// - для удобства
	}; // struct MsgTable
	static const MsgTable &msgTable() { static MsgTable t; return t; }
	RetCode m_rc;
	unsigned m_rcc;
	// - Конкретизация (case) кода возврата; позволяет найти место в программе
	bool m_crit;
}; // class RI

//! "Журнал ошибок".
/*! Кроме сведений о последней (критической) ошибке, на которой выполнение
	операции прервалось (эти сведения доступны через унаследованный класс RI),
	предоставляет (пока только) статистику ошибок в виде списка
	<код, частота>. */
class ErrorLog : public RI {
public:
//	RI getRetInfo() const { RI ri = *this; return ri; }
	RI getRetInfo() const { return static_cast<const RI &>(*this); }
	//!< Возвращает сведения о последней ошибке
	virtual unsigned frequency(RI::RetCode retCode) const = 0;
	//!< Возвращает частоту кода ошибки retCode
	virtual unsigned total() const = 0;
	//!< Возвращает общее число ошибок
};


//! DLIS-формат представления значений
namespace Representation {

//! Коды представления.
/*! DLIS-коды представления значений (%Representation codes). \n
	Описание форматов значений для всех кодов см.
		<a href="http://w3.energistics.org/RP66/V1/rp66v1_appb.html">
		здесь</a>. \n
	N/A - в данном модуле код не поддерживается. \n
	Примечание. Реальное соответствие типам C++ зависит от компилятора. */
enum Code {
	Undefined = 0, Auto = 0,  //!< Код по умолчанию (в стандарте DLIS его нет)
	FSHORT =  1, //!< N/A
	FSINGL =  2, //!< обычно соответствует float, 4 байта (формат IEEE)
	FSING1 =  3, //!< N/A
	FSING2 =  4, //!< N/A
	ISINGL =  5, //!< N/A
	VSINGL =  6, //!< N/A
	// - float, 4 байта (формат VAX), будет поддержка чтения
	FDOUBL =  7, //!< double, 8 байт (формат IEEE)
	FDOUB1 =  8, //!< N/A
	FDOUB2 =  9, //!< N/A
	CSINGL = 10, //!< N/A
	CDOUBL = 11, //!< N/A
	SSHORT = 12, //!< signed char, 1 байт
	SNORM  = 13, //!< short, 2 байта
	SLONG  = 14, //!< int, 4 байта
	USHORT = 15, //!< unsigned char, 1 байт
	UNORM  = 16, //!< unsigned int, 2 байт
	ULONG  = 17, //!< unsigned long, 4 байта
	UVARI  = 18, //!< unsigned long, 1, 2 или 4 байта
	IDENT  = 19, //!< string, переменная длина (идентификатор, класс Ident)
	ASCII  = 20, //!< string, переменная длина
	DTIME  = 21, //!< Дата и время, 8 байт (класс DateTime)
	ORIGIN = 22, //!< Ссылка на Origin-объект, переменная длина
	OBNAME = 23, //!< Имя объекта, переменная длина (класс ObjectName)
	OBJREF = 24, //!< Ссылка на объект, переменная длина (класс ObjectReference)
	ATTREF = 25, //!< N/A
	STATUS = 26, //!< bool, 1 байт
	UNITS  = 27, //!< string, переменная длина (единицы измерения, класс Units)
	Count
};

//! \cond
const uint32 maxUVARI = 0x3FFFFFFF;

// Функции определения подходящего Representation code по типу значения
//TODO: Неплохо было бы ввести функции типа 'bool isInteger(Code code)'
bool valid(Code code);
template <class T>
Code getProperCode();
template<> Code getProperCode<int>();
template<> Code getProperCode<double>();
//bool isInt(Code code) { /*...*/ }
//bool isFloat(Code code) /*...*/ }
//bool isStr(Code code) { /*...*/ }
// ...
//! \endcond

unsigned getSize(Code code);
//!< Возвращает размер DLIS-значения в байтах (или -1 при переменной длине)

// Функции конвертирования значений различных типов во внутренний формат DLIS
//	 и обратно согласно Representation code
//	 (ptrRawValue - указатель на первый байт DLIS-представления значения)
//RI toRawValue(const int &value, Code code, byte *ptrRawValue);
//RI fromRawValue(int &value, Code code, byte *ptrRawValue);
//// ...

}; // namespace Representation


//! Дата и время.
struct DateTime 
{
	//! Система измерения времени (буквально: "часовой пояс")
	enum TimeZone {
		LST,	//!< Local Standard Time
		LDT,	//!< Local Daylight Savings Time
		GMT		//!< Greenwich Mean Time
	};
	DateTime() :
		year(1900), timeZone(LST), month(1), day(1),
		hour(0), min(0), sec(0), msec(0) {}
	void setDate(uint16 yr, byte m = 1, byte d = 1) {
		year = yr;
		month = m;
		day = d;
	}
	//!< Устанавливает дату (время дня не меняется)
	void setTime(byte h, byte mn = 0, byte s = 0, uint16 ms = 0) {
		hour = h;
		min = mn;
		sec = s;
		msec = ms;
	}
	//!< Устанавливает время дня (дата не меняется)
	bool valid() const;	//!< Проверяет значение на соответствие стандарту DLIS
// Data
	uint16 year;		//!< Год (1900..2155)
	TimeZone timeZone;	//!< Часовой пояс (см. TimeZone)
	byte month;			//!< Месяц (1..12)
	byte day;			//!< День месяца (1..31)
	byte hour;			//!< Часы (0..23)
	byte min;			//!< Минуты (0..59)
	byte sec;			//!< Секунды (0..59)
	uint16 msec;		//!< Миллисекунды (0..999)
};

//! Базовый класс для классов Ident и Units.
/*! Класс для представления "коротких" строк (менее 256 символов).
	Длинные строки усекаются автоматически (в конструкторе и функции set()). */
class Symbol {
public:
	Symbol(const string &s = "") { set(s); }
	Symbol(const char *s) { set(s); }
	bool empty() const { return m_s.empty(); }
	const string &str() const { return m_s; }
	void set(const string &s) { m_s = s.substr(0, 255); }
	void clear() { set(""); }
	friend bool operator==(const Symbol &s1, const Symbol &s2) {
		return s1.m_s == s2.m_s;
	}
// - NB: если объявить operator== как член класса, возникают проблемы при
//		 выполнении сравнения в функции Attribute::Impl::Component::setCharc
private:
	string m_s;
};

//! Идентификатор.
/*! Класс на основе "коротких" строк, используемый для представления меток
	(имен) DLIS-атрибутов, как часть имен DLIS-объектов (класс ObjectName)
	и т.д. Соответствует коду представления IDENT. */
class Ident : public Symbol {
public:
	static bool valid(const string &s);
	Ident(const string &s = "") : Symbol(s) {}
	Ident(const char *s) : Symbol(s) {}
// - NB: Конструкторы (кроме конструкторов по умолчанию в некоторых случаях)
//		 не наследуются
	bool valid() const { return valid(str()); }
	//!< Функции valid проверяют значение на соответствие стандарту DLIS.
	/*!< Выполняют проверку строки на наличие символов, недопустимых для кода
		 IDENT (см.
			 <a href="http://w3.energistics.org/RP66/V1/rp66v1_appb.html#B_19">
			 стандарт DLIS</a>).
*/
};

//! Единица измерения.
/*! Класс на основе "коротких" строк для представления символьных обозначений
	единиц измерения. Соответствует коду представления UNITS. */
class Units : public Symbol 
{
public:
	static bool valid(const string &s);
	Units(const string &s = "") : Symbol(s) {}
	Units(const char *s) : Symbol(s) {}
	bool valid() const { return valid(str()); }
	//!< Функции valid проверяют значение на соответствие стандарту DLIS.
	/*!< Выполняют проверку строки на наличие символов, недопустимых для кода
		 UNITS (см.
			 <a href="http://w3.energistics.org/RP66/V1/rp66v1_appb.html#B_27">
			 стандарт DLIS</a>).
		 Корректность собственно формата символьного обозначения единицы
		 измерения в данной версии модуля не проверяется. */
};

class Origin;

//! Имя DLIS-объекта.
/*! Класс для представления имени DLIS-объекта (без указания его типа).
	Используется для ссылки на объекты, тип которых известен из контекста.
	Соответствует коду представления OBNAME. */
class ObjectName 
{
public:
	ObjectName() { clear(); }
	/*!< Создает экземпляр класса с пустыми полями */
	ObjectName(const string &idstr	/*!< строковый идентификатор объекта */,
			   uint32 originId		/*!< идентификатор ORIGIN-объекта */,
			   byte copyNum = 0		/*!< номер копии объекта */) {
		set(Ident(idstr), originId, copyNum);
	}
	/*!< Создает экземпляр класса, используя строковый идентификатор idStr. */
	ObjectName(const Ident &id, uint32 originId, byte copyNum) {
		set(id, originId, copyNum);
	}
	/*!< Создает экземпляр класса, используя идентификатор id типа Ident
	(см. тж. предыдущий конструктор) */
	bool valid() const 
    {
		return !m_id.empty() && m_id.valid() &&
			   m_ori <= Representation::maxUVARI;
	}

	//!< Проверяет значение на корректность.
	/*!< Проверяет корректность поля %Origin и идентификатора объекта,
		 а также то, не является ли он пустой строкой. */
	Ident ident() const { return m_id; }
	//!< Возвращает идентификатор объекта
	uint32 originId() const { return m_ori; }
	//!< Возвращает (числовой) идентификатор соответствующего ORIGIN-объекта
	byte copyNum() const { return m_cpy; }
	//!< Возвращает номер копии объекта
	bool operator==(const ObjectName &other) const {
		return m_ori == other.m_ori && m_cpy == other.m_cpy &&
			   m_id.str() == other.m_id.str();
		// - NB: см. примечание в ObjectReference::operator==
	}
	void set(const Ident &id, uint32 originId, byte copyNum) {
		m_ori = originId;
		m_cpy = copyNum;
		m_id = id;
	}
	/*!< Устанавливает поля экземпляра класса (см. конструктор) */
	void clear() { set(Ident(""), 0, 0); }
private:
	uint32 m_ori;
	byte m_cpy;
	Ident m_id;
};

//! Ссылка на DLIS-объект.
/*! Ссылка на DLIS-объект с указанием его типа. Соответствует коду
	представления OBJREF. */
class ObjectReference 
{
public:
	ObjectReference(const string &typeId /*!< идентификатор типа объекта*/,
					const ObjectName &obn /*!< имя объекта*/) :
		m_t(typeId), m_o(obn) {}
	const string &typeId() const { return m_t; }
	//!< Возвращает тип объекта
	const ObjectName &objectName() const { return m_o; }
	//!< Возвращает имя объекта
	bool valid() const { return !m_t.empty() && m_o.valid(); }
	//!< Проверяет значение на корректность.
	/*!< Проверяет корректность имени объекта, а также то, не является ли
		 значение типа пустой строкой. */
	bool operator==(const ObjectReference &other) const 
    {
		return m_t == other.m_t && m_o == other.m_o;
		// - NB: При условии, что сравниваемые экземпляры ObjectReference
		//		 совпадают редко, порядок сравнения членов (m_t и m_o)
		//		 на быстродействие практически не влияет
		//		 (эксперименты это подтверждают)
	}
private:
	string m_t;
	ObjectName m_o;
};


//! Интерфейс доступа к значению атрибута или текущему значению канала.
/*! Функции get и getVector возвращают значение атрибута/канала или
	один из элементов этого значения \n\n
	Функции set и setVector устанавливают значение атрибута/канала. \n
	Применительно к атрибуту (экземпляру Attribute) эти функции устанавливают
	значение атрибута вместе с его характеристиками Count (число элементов)
	и %Representation Code (код представления). Число элементов значения
	задается размером контейнера val или устанавливается в 1, если параметр val
	имеет скалярный тип (или тип std::string). Код представления задается
	параметром code. При этом значения обоих параметров должны соответствовать
	ограничениям согласно стандарту DLIS для данного атрибута. Если значение
	code равно Representation::Auto, код представления выбирается автоматически
	на основе типа параметра val и с учетом ограничений атрибута. \n
	Применительно к каналу (экземпляру Channel) эти функции устанавливают
	только его текущее значение, т. к. код представления и число элементов
	значения канала зафиксированы его атрибутами REPRESENTATION-CODE и
	DIMENSION. Поэтому при установке текущего значения канала единственно
	допустимым значением code является Representation::Auto. */
class ValueInterface {
public:
	typedef Representation::Code RCode;
	static const RCode rAuto = Representation::Auto;
/*! \name Read functions */
///@{
// doxy kludge
//! Шаблон для скалярного значения
	template <typename T>
	RI get(T &val) const {
		if (count() != 1) return RI(RI::FromRawNone, 7).toCritical();
		return get(&val, 0, 1);
	}
//! Шаблон для чтения скалярного элемента многомерного значения с индексом pos
	template <typename T>
	RI get(T &val, size_t &pos) const {
		if (pos >= count()) return RI(RI::BigValIndex).toCritical();
		return get(&val, pos, 1);
	}
//! Шаблон для контейнеров std::vector
	template <class T>
	RI get(vector<T> &vec) const {
		vec.resize(count());
		return get(&vec[0], 0, vec.size());
	}
//! Шаблон для шаблонов контейнеров с 1 параметром (std::valarray, QVector)
	template <class T, template<class> class Vector>
	RI get(Vector<T> &vec) const {
		vec.resize(count());
		if (!contiguous(vec)) return RI(RI::NonContig, 2).toCritical();
		return get(&vec[0], 0, vec.size());
	}
//! Шаблон для шаблонов контейнеров более чем с 1 параметром (как std::vector)
	template <class Vector>
	RI getVector(Vector &vec) const {
		vec.resize(count());
		if (!contiguous(vec)) return RI(RI::NonContig, 1).toCritical();
		return get(&vec[0], 0, vec.size());
	}
//	long asInt(size_t index = -1) const;
//	// - для кодов USHORT, SHORT, ...
//	double asFloat(size_t index = -1) const;
//	// - для кодов FSINGL, FDOUBL, ...
//	string asStr(size_t index = -1) const;
//	// - для кодов IDENT, ASCII, ...
///@}
/*! \name Write functions */
///@{
// doxy kludge
//! Шаблон для скалярных значений
	template <typename T>
	RI set(const T &val, RCode code = rAuto) {
		return set(&val, 1, code);
	}
//! Функция для литеральных строковых констант
	RI set(const char *val, RCode code = rAuto) {
		return set(&(string)val, 1, code);
	}
//! Шаблон для контейнеров std::vector
	template <class T>
	RI set(const vector<T> &vec, RCode code = rAuto) {
		return set(&vec[0], vec.size(), code);
	}
//! Шаблон для шаблонов контейнеров с 1 параметром (std::valarray, QVector)
	template <class T, template<class> class Vector>
	RI set(const Vector<T> &vec, RCode code = rAuto) {
		if (!contiguous(vec)) return RI(RI::NonContig, 2).toCritical();
		return set(&vec[0], vec.size(), code);
	}
//! Шаблон для шаблонов контейнеров более чем с 1 параметром (как std::vector)
	template <class Vector>
	RI setVector(const Vector &vec, RCode code = rAuto) {
		if (!contiguous(vec)) return RI(RI::NonContig, 2).toCritical();
		return set(&vec[0], vec.size(), code);
	}
///@}
private:
//! \cond
	friend class Attribute;
	friend class Channel;
	class        Dispatcher;

	template <class Container>
	static bool contiguous(const Container &c) 
    {
		if (c.size() <= 1) return true;
		size_t lastpos = c.size() - 1;
		return &c[0] + lastpos == &c[lastpos];
	}

	ValueInterface(const Dispatcher *vd);
	~ValueInterface();
	size_t count() const;
#define GET_SET(type)								\
	RI get(type *p, size_t pos, size_t cnt) const;	\
	RI set(const type *p, size_t cnt, RCode c);
	GET_SET(int8)
	GET_SET(byte)
	GET_SET(uint16)
	GET_SET(int16)
	GET_SET(uint32)
	GET_SET(int32)
	GET_SET(ieeeSingle)
	GET_SET(ieeeDouble)
	GET_SET(string)
	GET_SET(bool)
	GET_SET(Representation::Code)
	GET_SET(Ident)
	GET_SET(Units)
	GET_SET(ObjectName)
	GET_SET(ObjectReference)
	GET_SET(DateTime)
#undef GET_SET
	const Dispatcher *const m_pvd;
//! \endcond
}; // class ValueInterface

class   AttributeDefinition;

class   AttributeParent;

//! Атрибут DLIS-объекта.
class Attribute {
public:
	static const string &labelStrOf(const Attribute *pa);
//	const AttributeRestrictions &getRestrictions() const;
// Функции чтения
	const Ident &label() const;	//!< Метка (имя) атрибута
//	const string &labelStr() const { return label().str(); }
	uint32 count() const;
	//!< Число элементов значения (характеристика Count)
	Representation::Code representationCode() const;
	//!< Код представления значения (характеристика %Representation Code)
	const Units &units() const;
	//!< Единица измерения значения (характеристика %Units)
	const ValueInterface &cvalue() const;	//!< Интерфейс чтения значения.
	/*!< Предоставляет функции получения значения атрибута. */
//	template <class T> RI getValues(vector<T> &values) const;
//	std::valarray<byte> &rawValue() const {/*...*/}
//	// - Представление DLIS-значения в собственном формате DLIS
// Функции записи
//	RI setRepresentationCode(Representation::Code code);
// - NB: Representation Code устанавливается через value
	RI setUnits(const Units &units);
	//!< Устанавливает единицу измерения значения (характеристика %Units)
	ValueInterface &value();	//!< Интерфейс записи значения.
	/*!< Предоставляет функции установки значения атрибута вместе
		 с характеристиками Count и %Representation Code. */
//	template <class T> RI setValue(const T &value,
//	  Representation::Code code = Representation::Undefined) {/*...*/}
//	template <class T> RI setValue(const vector<T> &value,
//	  Representation::Code code = Representation::Undefined) {/*...*/}
//protected:
//// Функции низкоуровневой записи характеристик DLIS-значения (без проверки
////	 их взаимного соответствия
//	RI setValueCount(uint32 count) {/*...*/}
//	RI setRepresentationCode(Representation::Code code) {/*...*/}
//	void setRawValue(std::valarray<byte> &val) {/*...*/}
//	// - В функциях setCount и setRepresentationCode проверяется только
//	//	 соответствие ограничениям атрибута
private:
//! \cond
	template <class Container>
	friend void erasePtrContainer(Container &c,
								  typename Container::iterator first,
								  typename Container::iterator last);
	friend class ValueInterface;
	friend class BaseObjectImpl;
	friend class Template;
	friend class Object;
	friend class FileHeader;
	friend class Channel;
	friend class FrameType;
	friend class Set;
	static void deletePtr(Attribute *a) { delete a; }
	Attribute(AttributeParent *parent, const Ident &label = Ident(),
			  const AttributeDefinition *ad = NULL);
	~Attribute();
	class Impl;
	Impl *const pim;
//! \endcond
}; // class Attribute

class ObjectParent;

//! Базовый класс для классов DLIS-объектов.
class Object {
public:
	//! Типы DLIS-объектов.
	/*! Элементы данного перечисления соответствуют идентификаторам
		%Object types или, по-другому, Set types, которые возвращаются
		функциями Object::typeId. Полный список идентификаторов типов объектов,
		поддерживаемых форматом DLIS, см. в
			<a href="http://w3.energistics.org/RP66/V1/rp66v1_appa.html#A_2">
			таблице</a>
		(колонка Allowable Set Types). */
	enum Type {
		TypeUndefined,
		TypeAny = TypeUndefined,
		FILE_HEADER,
		ORIGIN,
		WELL_REFERENCE_POINT,
		AXIS,
		CHANNEL,
		FRAME,
		PATH,
		CALIBRATION,
		CALIBRATION_COEFFICIENT,
		CALIBRATION_MEASUREMENT,
		COMPUTATION,
		EQUIPMENT,
		GROUP,
		PARAMETER,
		PROCESS,
		SPLICE,
		TOOL,
		ZONE,
		COMMENT,
		MESSAGE,
//		UPDATE,
		TypeOther,
		TypeCount
	};
	static const string &typeId(Type type);
	static const ObjectName &nameOf(const Object *po);
	static ObjectReference referenceTo(const Object *po);
// Функции чтения
	const ObjectName &name() const;	//!< Имя объекта
	Type type() const;				//!< Тип объекта
	const string &typeId() const;	//!< Строковый идентификатор типа объекта
//	const vector<const Attribute *> &cattributes() const {/*...*/}
//	// - Возвращает список всех атрибутов объекта
	unsigned attributeCount() const;	//!< Количество атрибутов объекта
	typedef vector<const Attribute *>::const_iterator AttributeConstIt;
	//!< Константный итератор по атрибутам
	AttributeConstIt cbeginAttribute() const; //!< Итератор на первый атрибут
	AttributeConstIt cendAttribute() const;	  //!< Итератор на последний атрибут
	const Attribute *cattribute(const string &labelStr) const;
	//!< Атрибут с указанной меткой (для чтения)
// Функции записи
	Attribute *attribute(const string &labelStr);
	//!< Атрибут с указанной меткой (для записи).
	/*!< Если атрибут отсутствует (не был добавлен функцией
		 LogicalFile::introduceAttribute и не является фиксированным в данном
		 модуле для DLIS-объектов этого типа), функция возвращает NULL. */
	Attribute *stdAttribute(const string &labelStr);
	//!< Стандартный атрибут с указанной меткой (для записи).
	/*!< При отсутствии атрибута он добавляется автоматически (ко всем
		 DLIS-объектам этого типа). Если же, согласно стандарту, атрибута
		 с такой меткой у объектов этого типа нет, функция возвращает NULL. \n
		 См. тж. LogicalFile::introduceAttribute().
		   Пояснение. Операцию добавления атрибута можно представлять себе как
		 добавление колонки в таблицу атрибутов объектов данного типа. \n */
//	template <class T> RI addAttribute(Attribute * &attr, const Ident &label,
//	  const T &value, const Units &units = "", Representation::Code code = Representation::Undefined)
//	  {/*...*/}
//	/* - Добавляет атрибут вместе со значением, единицами измерения
//		 и кодом представления. При отсутствии последнего он определяется
//		 автоматически по типу T входного значения согласно ограничениям
//		 атрибута или результату Representation::getProperCode.
//		 Число значений (Count) устанавливается в 1 */
//	template <class T> RI addAttribute(Attribute * &attr, const Ident &label,
//	  const vector<T> &value, const Units &units = "",
//	  Representation::Code code = Representation::Undefined) {/*...*/}
//	/* - То же, со списком значений в контейнере value.
//		 Число значений (Count) устанавливатся по размеру value */
protected:
//! \cond
	friend class Attribute;
	friend class Set;
	friend class LogicalFile;
//	Object();
	Object(ObjectParent *parent, const ObjectName &name = ObjectName());
	virtual ~Object();
	RI addAttribute(Attribute * &attr) {/*...*/}
	// - Добавляет атрибут, созданный за пределами данного модуля
	//	 (см. ниже примечание к protected-функции Writer::startNewLogicalFile)
	class Impl;
	Impl *pim;
private:
	static void deletePtr(Object *o) { delete o; }
//! \endcond
}; // class Object

//! ORIGIN-объект (источник данных).
/*! Атрибуты DLIS-объекта типа ORIGIN содержат краткое описание источника
	данных (что примерно соответствует секции Well Information LAS-файла). \n
	Полный список атрибутов объекта ORIGIN см.
		<a href="http://w3.energistics.org/RP66/V1/rp66v1_sec5.html#5_2_1">
		здесь</a>. */
class Origin: public Object {
public:
	static Object::Type type() { return Object::ORIGIN; }
private:
//! \cond
	friend class Set;
	Origin(ObjectParent *parent, const ObjectName &name = ObjectName());
//! \endcond
};

//! CHANNEL-объект (канал).
/*! Атрибуты DLIS-объекта типа CHANNEL содержат название и формат значений
	канала (что примерно соответствует описанию кривой в секции
	Curve Information LAS-файла). \n
	Атрибуты REPRESENTATION-CODE, UNITS, DIMENSION и ELEMENT-LIMIT создаются
	автоматически. \n
	Полный список атрибутов объекта CHANNEL см.
		<a href="http://w3.energistics.org/RP66/V1/rp66v1_sec5.html#5_5_1">
		здесь</a>. */
class Channel: public Object 
{
public:
	static Object::Type type() { return Object::CHANNEL; }
// Функции чтения основных атрибутов
	Representation::Code representationCode() const;
	//!< Код представления значения канала (атрибут REPRESENTATION-CODE)
	Units &units() const;
	//!< Единица измерения значения канала (атрибут UNITS)
	const vector<uint32> &dimension() const;
	//!< Размерность значения канала (атрибут DIMENSION).
	/*!< dimension().size() равно числу измерений: \n
		 1 - для скалярных (одноэлементных) и векторных значений, \n
		 2 - для матричных значений, и т.д. \n
		 Для скалярных значений dimension()[0]==1. */
// Функции интерпретации атрибута Dimension
	size_t flatCount() const;	//!< Общее число значений канала.
	/*!< Равно произведению всех элементов атрибута Dimension. */
//	bool isScalar() const {/*...*/}
//	// - Возвращает true в случае скалярного значения
//	bool isVector() const {/*...*/}
//	// - Возвращает true в случае векторного значения (не-скалярного значения
//	//	 с числом измерений 1)
// Функции установки основных атрибутов
	RI setRepresentationCode(Representation::Code code);
	//!< Устанавливает код представления значения канала (атрибут REPRESENTATION-CODE)
	RI setUnits(const Units &units);
	//!< Устанавливает единицу измерения значения канала (атрибут UNITS)
	RI setDimension (uint32 dim);
	//!< Устанавливает размерность векторного значения канала (атрибут DIMENSION)
	RI setDimension (const vector<uint32> &dims);
	//!< Устанавливает произвольную размерность значения канала (атрибут DIMENSION)
	// - Функции setDimension автоматически устанавливают в то же значение
	//	 и атрибут ELEMENT-LIMIT (однако в случае прямой установки атрибута
	//	 DIMENSION через Attribute::value::set значение ELEMENT-LIMIT
	//	 не изменяется )
// Функции получения и установки текущих значений канала (в текущем фрейме)
//	// - Функции getCurrentValue возвращают скалярное значение или один из
//	//	 элементов многомерного значения.
//	/* - Параметр flatIndex указывает для векторных значений обычный индекс
//		 элемента, для матричных значений - индекс в воображаемом векторе,
//		 получаемом "конкатенацией" строк матрицы значений, и т.д. Если этот
//		 параметр опущен (равен -1), значение должно быть скалярным */
	const ValueInterface &ccurrentValue() const;
	//!< Интерфейс чтения текущего значения канала
	ValueInterface &currentValue();
	//!< Интерфейс записи текущего значения канала
private:
//! \cond
	friend class ValueInterface;
	friend class Set;
	friend class FrameType;
	friend class Frame;
//	friend class LogicalFile;
	Channel(ObjectParent *parent, const ObjectName &name = ObjectName());
	~Channel();
	class Impl;
	Impl *const pim;
//! \endcond
}; // class Channel

//! FRAME-объект (тип фрейма).
/*! Атрибуты DLIS-объекта типа FRAME содержат список каналов и описание формата
	их значений в далее идущих фреймах. \n
	Примечание. Фреймом называется массив значений указанных каналов для одного
	значения глубины / момента времени (что примерно соответствует одной строке
	LAS-файла). \n
	Атрибут CHANNELS создается автоматически. \n
	Полный список атрибутов объекта FRAME см.
		<a href="http://w3.energistics.org/RP66/V1/rp66v1_sec5.html#5_7_1">
		здесь</a>. \n
	Примечение. В описании по этой ссылке есть ошибка: в таблице возможных
	значений атрибута INDEX-TYPE должно присутствовать значение TIME
	(с описанием "Index measures elapsed time."). */
class FrameType: public Object {
public:
	static Object::Type type() { return Object::FRAME; }
// Функции чтения
	const vector<const Channel *> &cchannels() const;
	//!< Список каналов (для чтения значений)
	const uint32 frameCount() const;	//!< Количество фреймов данного типа
// Функции записи
	RI setChannels(const vector<ObjectName> &channelNames);
	//!< Устанавливает список каналов (атрибут CHANNELS)
// doxy kludge
//! \cond
	RI setChannels(const vector<Channel *> &channels);
//! \endcond
	const vector<Channel *> &channels();
	//!< Список каналов (для записи значений)
private:
//! \cond
	friend class Set;
	friend class LogicalFile;
	friend class Reader;
	FrameType(ObjectParent *parent, const ObjectName &name = ObjectName());
	~FrameType();
	class Impl;
	Impl *const pim;
//! \endcond
};

class LogicalFileParent;

//! Логический файл.
/*! Содержит метаданные (DLIS-объекты) и значения каналов. Примерно
	соответствует LAS-файлу версии 2.0. */
class LogicalFile 
{
private:
	class ObjectIteratorData;
	ObjectIteratorData *copy(const ObjectIteratorData &d) const;
	bool same(const ObjectIteratorData &d1, const ObjectIteratorData &d2) const;
	const Object *cgetObject(const ObjectIteratorData &d) const;
	Object *getObject(const ObjectIteratorData &d);
	void iterateObject(ObjectIteratorData &d, bool fwd /*backwards = false*/) const;
public:
// Функции чтения
	uint32 seqNum() const;
	//!< Порядковый номер логического файла.
	/*!< См. тж. Writer::startNewLogicalFile(). */
	const string &id() const;
	//!< Идентификатор-описатель логического файла.
	/*!< Значение атрибута ID без пробелов в конце строки. \n
		 См. тж. фукнкцию setId(). */
	uint32 definingOriginId() const;
	//!< Идентификатор Defining Origin.
	/*!< Числовой идентификатор ORIGIN-объекта Defining %Origin. */
	bool anyEncrypted() const;
	//!< Признак наличия зашифрованных данных
	typedef ConstPtrContainerIterator<LogicalFile, Object, ObjectIteratorData,
									  &cgetObject, &iterateObject,
									  &copy, &same> ObjectConstIt;
	//!< Константный итератор по DLIS-объектам
	ObjectConstIt cbeginObject(Object::Type type = Object::TypeAny) const;
	//!< Итератор на первый DLIS-объект типа type.
	/*!< Значение Object::TypeAny используется для прохода по объектам всех
		 типов. */
	ObjectConstIt cendObject() const;
	//!< Итератор на последний DLIS-объект.
	/*!< Примечание. Возвращаемое функцией cendObject значение одинаково для
		 всех типов DLIS-объектов. */
/*! \name Getting objects
	Функции cobject возвращают константный DLIS-объект указанного типа
	и с указанным именем. \n
	Тип задается параметром type обычной функции или параметром SpecificObject
	шаблона функции (могут быть использованы классы Origin, Channel
	и FrameType). \n
	Имя объекта задается целиком в параметре name или покомпонентно --
	параметрами nameIdStr, originId и copyNum (см. класс ObjectName).
	Если параметр originId равен -1, используется идентификатор	ORIGIN-объекта
	Defining %Origin. */
///@{
	const Object *cobject(Object::Type type, const ObjectName &name) const;
	const Object *cobject(Object::Type type, const string &nameIdStr,
						  uint32 originId = -1, byte copyNum = 0) const;
	template <class SpecificObject>
	const SpecificObject *cobject(const ObjectName &name) const;
	template <class SpecificObject>
	const SpecificObject *cobject(const string &nameIdStr,
								  uint32 originId = -1, byte copyNum = 0) const;
///@}
	const Origin *cdefiningOrigin() const;
	//!< Объект Defining Origin (для чтения)
// Функции записи
	RI setId(const string &id);
	//!< Задает идентификатор-описатель логического файла.
	/*!< Задает значение атрибута ID объекта File Header. Максимальная длина --
		 65 символов. */
	Origin *definingOrigin();
	//!< Объект Defining %Origin (для записи)
/*! \name Adding objects
	Функции addNewObject добавляют в логический файл новый (пустой) DLIS-объект
	указанного типа	и с указанным именем. \n
	Параметры функций аналогичны параметрам функций cobject. */
///@{
	Object *addNewObject(Object::Type type, const ObjectName &name,
						 RI *ri = NULL);
	Object *addNewObject(Object::Type type, const string &nameIdStr,
						 uint32 originId = -1, byte copyNum = 0, RI *ri = NULL);
	template <class SpecificObject>
	SpecificObject *addNewObject(const ObjectName &name, RI *ri = NULL);
	template <class SpecificObject>
	SpecificObject *addNewObject(const string &nameIdStr, uint32 originId = -1,
								 byte copyNum = 0, RI *ri = NULL);
///@}
	RI introduceAttribute(Object::Type type, const string &labelStr,
						  bool standardOnly = true);
	//!< Добавляет новый атрибут.
	/*!< Добавляет новый атрибут с меткой labelStr для всех DLIS-объектов типа
		 type. Чтобы добавить собственный атрибут (не описанный в стандарте),
		 значение параметра standardOnly должно быть false. \n
		 См. тж. Object::stdAttribute(). */
protected:
//! \cond
	RI addObject(Object * &object, ObjectName &name) {/*...*/}
	// - Добавляет DLIS-объект, созданный за пределами данного модуля
	//	 (см. ниже примечание к protected-функции Writer::startNewLogicalFile)
private:
	friend class FrameType;
	// - для доступа к ObjectIt
	friend class Reader;
	friend class Writer;
	static void deletePtr(LogicalFile *lf) { delete lf; }
	LogicalFile(LogicalFileParent *parent);
	LogicalFile(LogicalFileParent *parent,
				uint32 seqNum, const ObjectName &defOri);
	~LogicalFile();
	typedef PtrContainerIterator<LogicalFile, Object, ObjectIteratorData,
								 &getObject, &iterateObject,
								 &copy, &same> ObjectIt;
	// - Константный итератор по DLIS-объектам
	ObjectIt beginObject(Object::Type type = Object::TypeAny);
	ObjectIt endObject(Object::Type type = Object::TypeAny);
	class Impl;
	Impl *const pim;
//! \endcond
}; // class LogicalFile
// - Примечание. Удаление добавленных DLIS-объектов не предусмотрено

template <class SpecificObject>
SpecificObject *LogicalFile::addNewObject(const ObjectName &name, RI *ri) 
{
	Object *po = addNewObject(SpecificObject::type(), name, ri);
	return dynamic_cast<SpecificObject *>(po);
}

template <class SpecificObject>
SpecificObject *LogicalFile::addNewObject(const string &nameStr,
										  uint32 originId, byte copyNum,
										  RI *ri) 
{
	Object *po = addNewObject(SpecificObject::type(), nameStr,
							  originId, copyNum, ri);
	return dynamic_cast<SpecificObject *>(po);
}

template <class SpecificObject>
const SpecificObject *LogicalFile::cobject(const ObjectName &name) const {
	const Object *po = cobject(SpecificObject::type(), name);
	return dynamic_cast<const SpecificObject *>(po);
}

template <class SpecificObject>
const SpecificObject *LogicalFile::cobject(const string &nameIdStr,
										   uint32 originId,
										   byte copyNum) const {
	const Object *po = cobject(SpecificObject::type(), nameIdStr,
							   originId, copyNum);
	return dynamic_cast<const SpecificObject *>(po);
}


//! Класс для чтения DLIS-данных.
/*! Поддерживает чтение единицы хранения DLIS-данных (Storage Unit) из файла
	или потока. */
class Reader 
{

public:
    Reader();	//!< Создает пустой экземпляр Reader
    ~Reader();	//!< В случае работы с файлом закрывает файл
/*! \name Open functions
	Функции open начинают чтение Storage Unit чтением ее заголовка
	(Storage Unit Label). См. тж. функцию storageSetId(). */
///@{
	RI open(const string &fileName);
	//!< Начинает чтение из файла с именем fileName
	RI open(std::istream *stream, std::streamsize byteCount = -1);

	//!< Начинает чтение из потока *stream.
	/*!< Поток должен быть открыт для чтения и установлен на начало
		 DLIS-данных. \n
		 Параметр byteCount указывает число считываемых байтов.
		 Если он равен -1, чтение будет выполняться до конца потока. */
///@}
	void close();	//!< В случае работы с файлом закрывает файл
//	bool isDlis() const;
//	// - Возвращает true, если загруженные данные содержат корректный
//	//	 заголовок формата DLIS версии 1
	const string &storageSetId() const;
	//!< Идентификатор Storage Set (из Storage Unit Label).
	/*!< Строго говоря (по стандарту DLIS) Storage Set состоит из нескольких
		 элементов Storage Unit, но обычно, а также в данной реализации,
		 число элементов равно одному, и тогда понятия Storage Set
		 и Storage Unit совпадают. */
	unsigned logicalFileCount() const;	//!< Количество логических файлов
	typedef vector<const LogicalFile *>::const_iterator LogicalFileConstIt;
	//!< Константный итератор по логическим файлам
	LogicalFileConstIt cbeginLogicalFile() const;
	//!< Возвращает константный итератор на первый логический файл
	LogicalFileConstIt cendLogicalFile() const;
	//!< Возвращает константный итератор на последний логический файл
	const ErrorLog &read(bool loadFrames = true);
	//!< Считывает все логические файлы
	/*!< Считывает все логические файлы из файла/потока, ранее заданного
		 функцией read и возвращает ссылку на "журнал ошибок" -- список
		 сообщений об ошибках в прочитанных данных. \n
		 Метаданные загружаются в память полностью, а режим чтения фреймов
		 (которые содержат собственно значения каналов) определяется параметром
		 loadFrames. \n
		 Если loadFrames=true, фреймы загружаются в память полностью. \n
		 Если loadFrames=false, функция считывает заголовки всех фреймов,
		 строя таблицу их расположения для обеспечения последующего быстрого
		 произвольного доступа при вызове функции readFrame(). */
/* - Примечание. Разделить (в целях оптимизации) операции чтения метаданных
	 и фреймов невозможно, т. к. для определения начала (и самого факта
	 наличия) второго логического файла (и следующих за ним) нужно "пройти"
	 все фреймы предшествующего.
		- Оптимизация все-таки возможна, т.к. логические файлы начинаются
		  на границах видимых записей, которые, по сравнению с фреймами, обычно
		  (но не обязательно!) длиннее (что уменьшает число считываемых
		  заголовков) и устроены проще. */
	RI readFrame(const FrameType *frameType, uint32 frameIndex);
	//!< Загружает фрейм типа frameType и с индексом frameIndex.
	/*!< Из последовательности фреймов с типом, заданным FRAME-объектом
		 *frameType, загружает фрейм с индексом frameIndex.
		 Значения каналов фрейма можно получить вызовами функции
		 сurrentFrameValue() соответствующих экземпляров Channel. */
private:
	class Impl;
	Impl *const pim;
};

//! Класс для записи DLIS-данных.
/*! Поддерживает запись единицы хранения DLIS-данных (Storage Unit) в файл
	или поток. */
class Writer {
public:
	Writer();	//!< Создает пустой экземпляр Writer
	~Writer();	//!< В случае работы с файлом закрывает файл
/*! \name Open functions
	Функции open начинают запись Storage Unit записью ее заголовка
	(Storage Unit Label). \n
	В заголовок будет включен идентификатор (см. Reader::storageSetId()),
	заданный строкой id. Длина строки не должна превышать 60 символов. */
///@{
	RI open(std::ostream *stream, const string &id);
	//!< Начинает запись в поток *stream.
	/*!< Поток должен быть открыт для записи. */
	RI open(const string &fileName, const string &id);
	//!< Начинает запись в файл с именем fileName
// doxy kludge
///@}
	void close();
	//!< В случае работы с файлом закрывает файл
	LogicalFile *startNewLogicalFile(
			uint32 seqNum = 0,
				//!< Порядковый номер логического файла
			const ObjectName &definingOrigin = ObjectName("DEFINING_ORIGIN", 0),
				//!< имя ORIGIN-объекта Defining %Origin
			RI *retInfo = NULL);
	//!< Начинает запись нового логического файла.
	/*!< Создает новый логический файл и добавляет в него объекты File Header
		 и Defining %Origin. \n
		 Параметр seqNum задает значение атрибута SEQUENCE-NUMBER объекта
		 File Header. Он может быть любым положительным числом,
		 большим порядкового номера логического файла, предшествующего ему
		 в данном Storage Unit. Если значение seqNum не задано (равно 0), оно
		 устанавливается автоматически. Впоследствии это значение не может быть
		 изменено. \n
			Примечание 1. Возвращаемый признак ошибки (в параметре retInfo)
		 относится только входным параметрам, поэтому, если они имеют значения
		 по умолчанию, параметр retInfo не несет информации и может быть опущен
		 (вместе с остальными параметрами). \n
			Примечание 2. Признаком конца логического файла является начало
		 следующего, поэтому в функции завершения записи текущего логического
		 файла нет необходимости. */
	RI writeMetadata(/*vector<Attribute *> fwdAttrs = vector<Attribute *>()*/);
	//!< Записывает метаданные (все DLIS-объекты) текущего логического файла.
	/*!< Для каждого логического файла функция может быть вызвана только один
		 раз. */
	/* - Список fwdAttrs (по умолчанию пустой) задает атрибуты, значения
		 которых в дальнейшем (в процессе записи фреймов) могут быть
		 перезаписаны функцией rewriteAttribute. Возможность перезаписи
		 значений полезна, например, для занесения значений атрибутов INDEX-MIN
		 и INDEX-MAX в уже записанные FRAME-объекты, когда эти значения заранее
		 неизвестны, а определяются только в процессе записи фреймов,
		 что бывает при записи в реальном времени, по мере поступления данных
		 для очередных фреймов */
	RI writeNextFrame(FrameType *frameType);
	//!< Записывает очередной фрейм указанного типа.
	/*!< Записывает очередной фрейм из серии фреймов текущего логического файла
		 с типом, заданным FRAME-объектом *frameType. Значения каналов должны
		 быть предварительно загружены вызовами функций currentValue().set()
		 для соответствующих экземпляров Channel. \n
		 Примечание. Загруженные значения каналов после записи не очищаются и
		 будут записываться в следующие фреймы до тех пор, пока не будут
		 заменены новыми значениями. */
//! \cond
//	RI rewriteAttribute(Attribute *attr) {/*...*/}
//	// - Перезаписывает значение одного из атрибутов, переданных при вызове
//	//	 writeMetaData. Установка новых значений выполняется обычным способом.
//	RI rewriteAttributes() {/*...*/}
//	// - Перезаписывает значения всех таких атрибутов
	size_t writtenByteCount() {/*...*/}
	// - Число записанных байт
//! \endcond
//protected:
//	RI startNewLogicalFile(LogicalFile *lgcFile) {/*...*/}
//	/* - Подключает экземпляр класса LogicalFile (или его наследника),
//		 созданный за пределами данного модуля, в качестве нового логического
//		 файла и добавляет в него объект Defining Origin).
//		 Таким образов, определив новый класс - наследник Writer, затем новый
//		 класс - наследник LogicalFile и т.д., можно получить доступ
//		 к protected-членам классов данного модуля */
//	RI writeObject(Object *object) {/*...*/}
//	/* - Записывает отдельный DLIS-объект. Функция может быть использована,
//		 например, для записи UPDATE-объектов, изменяющих характеристики
//		 ранее созданных CHANNEL- и FRAME-объектов, "внутри" серии фреймов,
//		 т.е. между вызовами writeNextFrame */
private:
	class Impl;
	Impl *const pim;
};


//class Generic {
//private:
//	template <class Container>
//	static void erasePtrContainer(Container &c,
//								  typename Container::iterator first,
//								  typename Container::iterator last) {
//		for (Container::iterator it = first; it != last; ++it) delete *it;
//		c.erase(first, last);
//	}
//	template <class Container>
//	static void clearPtrContainer(Container &c) {
//		erasePtrContainer(c, c.begin(), c.end());
//	}
//// - NB: Хорошо бы как-то указать в аргументах шаблонов XXXPtrContainer,
////		 что они применимы только к контейнерам указателей
//};

//! \cond
#ifndef NDEBUG
// Функции внутреннего отладочного тестирования модуля
bool d_test_IntField();
bool d_test_ValueConvertor();
#endif // NDEBUG
//! \endcond


}; // namespace Dlis

#endif // DLIS
