#pragma warning(push)
#pragma warning(disable : 4018 4101 4150 4715)

// Модуль поддержки чтения и записи данных в формате DLIS 1.0
//	Автор: Александр Газизулин (TGT Oil & Gas Services)
//	Язык: C++03
//	Используемые библиотеки: нет
//	Версия: 0.1.3
//	Дата: 12.01.2016


/* Изменения относительно версии 0.1.1

	   Версия 0.1.2 (16.12.2015)
   - Шаблонная функция Dlis::LogicalFile::addNewObject с 4 параметрами
	 (и нешаблонная с 5 параметрами) не заполняла RI-параметр (хотя в случае
	 ошибки эти функции корректно возвращают NULL).
   -> исправлено
   Изменения не затрагивают заголовочный файл.

	   Версия 0.1.3 (12.01.2016)
   - Допускается (по примеру Schlumberger) использование символа процента '%'
	 в значениях с кодом представления UNITS (в нарушение стандарта DLIS 1.0).
	 Теперь при наличии символа '%' функции Units::valid() возвращают true,
	 а фунция ValueInterface::Dispatcher::setValue() для кода представления
	 UNITS больше не возвращает ошибку RI::WrongValType.
   Изменения не затрагивают заголовочный файл.
*/


//	Секция реализации


#include <cfloat>
#include <algorithm> // std::min
#include <sstream>
#include <bitset>
#include <iomanip>	 // std::setw
#include <cassert>

#include "Dlis.h"

#ifndef NDEBUG
//	extern int d_nfr = 0;
//	void d_nfr290(const char *s) {
//		if (d_nfr==290) qDebug() << s;
//	}
#endif // NDEBUG

namespace Dlis {

using std::string;
using std::vector;
using std::list;

typedef std::vector<byte> Bytes;
typedef unsigned int uint;


template <typename T>
inline size_t elemCount(const T &v) { return 1; }
template <typename T>
inline size_t elemCount(const std::vector<T> &v) { return v.size(); }

//template <typename T>
//inline bool isVector(const T &v) { return false; }
//template <typename T>
//inline bool isVector(const std::vector<T> &v) { return true; }


// Поиск в контейнере с элементами любого вида
template<class InputIt, class GetKeyPredicate, class KeyType>
InputIt findByKey(InputIt first, InputIt last, GetKeyPredicate keyOf, const KeyType &key) 
{
	for (; first != last; ++first) 
    {
		if (keyOf(*first) == key) 
            return first;
	}
	return last;
}

//// Поиск в контейнере с элементами - не указателями
//template<class InputIt, typename Key>
//// - NB: обойтись без параметра T, хотя он зависит от InputIt, не получается
//InputIt findByKey(InputIt first, InputIt last,
//				  Key (InputIt::value_type:: *getkey)() const, const Key &key) {
//	for (; first != last; ++first) {
//		if (((*first).*getkey)() == key) return first;
//	}
//	return last;
//}

//// Поиск в контейнере с элементами - указателями типа T*
//template<class InputIt, class T, typename Key>
//// - NB: обойтись без параметра T, хотя он зависит от InputIt, не получается
//InputIt findPtrByKey(InputIt first, InputIt last,
////					 const Key (T:: *getkey)() const, const Key &key) {
//					 const Key (T:: *&getkey)() const, const Key &key) {
////					 Key (T:: *getkey)(), const Key &key) {
//	for (; first != last; ++first) {
//		if (((**first).*getkey)() == key) return first;
//	}
//	return last;
//}

//template<class InputIt, class GetKeyPredicate>
//bool allUniqueKeys(InputIt first, InputIt last, GetKeyPredicate keyOf) {
//	if (first == last) return true;
//	InputIt predLast = last;
////		--predLast;
//// - NB: Декремент predLast является оптимизацией; отказываемся от нее, пока
////		 итератор ObjectConstIt не поддерживает оператор декремента
//	for (; first != predLast; ++first) {
//		int cnt = 0;
//		for (InputIt cur = first; cur != last; ++cur) {
//			if (keyOf(*first) == keyOf(*cur)) ++cnt;
//			if (cnt > 1) return false;
//		}
//	}
//	return true;
//}
template<class InputIt, typename GetKeyPredicate>
bool allUniqueKeys(InputIt first, InputIt last, GetKeyPredicate keyOf) 
{
//	if (first == last) return true;
//	InputIt predLast = last;
//	--predLast;
//	for (; first != predLast; ++first) {
/* - NB: Использование predLast является оптимизацией; отказываемся от нее, пока
		 итератор ObjectConstIt не поддерживает оператор декремента (а также
		 чтобы не ограничивать применение шаблона forward-итераторами) */
	for (; first != last; ++first) 
    {
		InputIt next = first;
		if (findByKey(++next, last, keyOf, keyOf(*first)) != last) 
            return false;
		// - NB: Ситуация равенства входных итераторов в findByKey предусмотрена
	}
	return true;
}

template<class InputIt>
bool allUnique(InputIt first, InputIt last) {
//	if (first == last) return true;
//	InputIt predLast = last;
//	--predLast;
//	for (; first != predLast; ++first) {
/* - NB: Использование predLast является оптимизацией; отказываемся от нее, пока
		 итератор ObjectConstIt не поддерживает оператор декремента (а также
		 чтобы не ограничивать применение шаблона forward-итераторами) */
	for (; first != last; ++first) {
		InputIt next = first;
		if (find(++next, last, *first) != last) return false;
		// - NB: Ситуация равенства входных итераторов в findByKey предусмотрена
	}
	return true;
}
//template <class T> void clearPtrVec(std::vector<T *> &v) {
//	for (typename std::vector<T *>::iterator p = v.begin(); p != v.end(); ++p)
//		{ delete *p; }
//	v.clear();
//}
//// - NB: Протестировано в проекте Contnrs
//template <class T>
//void erasePtrVec(std::vector<T *> &v, size_t first, size_t last) {
//	if (first >= last) return;
//	for (size_t n = first; n < last; ++n) delete v[n];
//	v.erase(v.begin() + first, v.begin() + last);
//}
// - Заменены шаблонами более общего вида XXXPtrContainer

template <class Container>
void erasePtrContainer(Container &c,
					   typename Container::iterator first,
					   typename Container::iterator last) {
	for (Container::iterator it = first; it != last; ++it) delete *it;
	c.erase(first, last);
}

template <class Container>
void clearPtrContainer(Container &c) {
	erasePtrContainer(c, c.begin(), c.end());
}

template <class Container>
void erasePtrContainer(Container &c,
					   void (*deletePtr)(typename Container::value_type),
					   typename Container::iterator first,
					   typename Container::iterator last) {
	for (Container::iterator it = first; it != last; ++it) deletePtr(*it);
	c.erase(first, last);
}

template <class Container>
void clearPtrContainer(Container &c,
					   void (*deletePtr)(typename Container::value_type)) {
	erasePtrContainer(c, deletePtr, c.begin(), c.end());
}

// - NB: Удобно было бы объединить пары шаблонов XXXPtrContainer, используя
//		 значение по умолчанию, однако это, видимо, трудно сделать одновременно
//		 просто и надежно (так, нельзя использовать значение NULL,
//		 а, например, существование и результат конструктора итератора
//		 по умолчанию в стандарте C++ не определен)
// - NB: Возможно, правильно было бы как-то указать в аргументах шаблонов
//		  XXXPtrContainer, что они применимы только к контейнерам указателей


void reverseBytes(byte *pBytes, uint cnt) {	// Число байт не больше maxCnt!

	const uint maxCnt = 8;
	if (cnt > maxCnt) 
        throw RI(RI::ProgErr, 1);

	static byte pTmp[maxCnt];
	if (cnt > maxCnt) 
        return;

	byte *p  = pBytes + cnt;
	byte *p2 = pTmp - 1;

	while (p != pBytes) 
    {
		*(++p2) = *(--p);
	}

	memcpy(pBytes, pTmp, cnt);
}

template <typename T> void reverseBytes(T &v) {
	reverseBytes((byte *)&v, sizeof v);
}
// - только для переменных размером не более 8 байт!

void d_test_reverseBytes_on_const() {
	int i;
	const int ic = 4;
	reverseBytes(i);
	reverseBytes(ic);
// - NB: нет ошибки (даже предупреждения) компилятора, хотя ic - константа
// ******************************************************
// ?????????????????????????????!!!!!!!!!!!!!!!!!!!!!!!!!
// ******************************************************
}

template <typename T>
void copy(T &v, const byte *p) {
	memcpy(&v, p, sizeof v);
}

template <typename T>
void copy(byte *p, const T &v) {
	memcpy(p, &v, sizeof v);
}

//void copy(std::ostream &os, std::istream &is, std::streamsize count) {
//	if (count <= 0) return;
//	std::streamsize maxBufSize = 0xF000;
//	int cnt = count;
//	std::streamsize bufSize = std::min(count, maxBufSize);
//	vector<byte> buf(bufSize);
//	char *pBuf = (char *)&buf[0];
//	while (count != 0) {
//		int n = std::min(count, bufSize);
//		is.read(pBuf, n);
//		os.write(pBuf, n);
//		count -= n;
//	}
//}
void copy(std::ostream &os, std::istream &is, std::streamsize count) {
	if (count <= 0) return;
	static const uint bufSize = 0xF000;
	static char buf[bufSize];
	size_t bytesLeft = count;
	while (bytesLeft != 0) {
		uint n = std::min(bytesLeft, bufSize);
		is.read(buf, n);
		os.write(buf, n);
		bytesLeft -= n;
	}
}
// - NB: За основу взят алгоритм из библиотеки Delphi7 VCL (TStream.CopyFrom)


// Функции обработки строк

bool toChars(char *p, size_t charCnt, const string &s,
			 bool allowTruncate = false) {
	size_t sLen = s.length();
	if (!allowTruncate && sLen > charCnt) return false;
	memcpy(p, s.data(), std::min(sLen, charCnt));
	if (sLen < charCnt) memset(p + sLen, ' ', charCnt - sLen);
	// - дополняем при необходимости пробелами
	return true;
}

bool strToInt(const string &s, long &num/*, bool leftJustified = false*/) {
	std::istringstream is(s);
	is >> num;
	return !is.fail();
/* ToDo: учесть leftJustified! */
}
bool charsToInt(const char *p, unsigned charCnt, long &num
			  /*, leftJustified = false*/) {
	return strToInt(string(p, charCnt), num);
/* ToDo: учесть leftJustified! */
}

string intToStr(long num, unsigned minWidth = 0/*, bool leftJustified = false*/) {
	std::ostringstream os;
	if (minWidth != 0) os << std::setw(minWidth);
	os << num;
	return os.str();
/* ToDo: учесть leftJustified! */
}
// - NB: Эксперименты показывают, что при нехватке места (width) выводимое
//		 значение не усекается и длина выводимой строки становится больше width
//		 (однако задавать значение width всегда полезно, т.к. оно определяет
//		 МИНИМАЛЬНУЮ ширину поля)
bool intToChars(char *p, unsigned charCnt, long num
				/*, leftJustified = false*/) {
	return toChars(p, charCnt, intToStr(num, charCnt));
	/* ToDo: учесть leftJustified! */
}
// - NB: Возвращает false, если для представления числа не хватило места

// Функции, отбрасывающие пробелы в конце строки
void eraseTrailingSpaces(string &s) {
	std::size_t last = s.find_last_not_of(" ");
	if (last != string::npos) s.erase(last + 1);
	else s.clear();
}
string getEraseTrailingSpaces(const string &s) {
	string res = s;
	eraseTrailingSpaces(res);
	return res;
}


bool operator<(const RI &ri1, const RI &ri2) {
	if (ri1.m_rc == ri2.m_rc) return ri1.m_rcc > ri2.m_rcc;
/* - NB: Для удобства, в отличие от "базовых" кодов m_rc, уточняющие коды
		 m_rcc с бОльшими значениями считаются менее серьезными (такой порядок
		 выглядит более естественным, т.к. проверки на самые серьезные ошибки
		 обычно выполняются в первую очередь) */
	else return ri1.m_rc < ri2.m_rc;
}

string RI::getMessage() const {
	std::ostringstream oss(message(m_rc), std::ios_base::ate);
	if (m_rcc) oss << " (Case " << m_rcc << ")";
	return oss.str();
}

RI &RI::upTo(const RI &ri) {
	if (*this < ri)	*this = ri;
	return *this;
}

class ErrorLogImpl : public ErrorLog {
// - NB: базовый класс RI определяет основную ошибку
public:
	typedef std::map<RI::RetCode, size_t> StatTable;
	typedef StatTable::value_type StatPair;
	uint frequency(RI::RetCode retCode) const;
	uint total() const;
	void clear() {
		ErrorLog::clear();
		mm_stat.clear();
	}
	void add(const RI &ri);
private:
	StatTable mm_stat;
};

uint ErrorLogImpl::frequency(RI::RetCode retCode) const {
	StatTable::const_iterator itPair = mm_stat.find(retCode);
	return itPair != mm_stat.end() ? itPair->second : 0;
}

uint ErrorLogImpl::total() const {
	uint total = 0;
	StatTable::const_iterator it = mm_stat.begin();
	for (; it != mm_stat.end(); ++it) total += (*it).second;
	return total;
}

void ErrorLogImpl::add(const RI &ri) {
	if (ri.ok()) return;
	std::pair<StatTable::iterator, bool> insertInfo =
			mm_stat.insert(std::make_pair(ri.retCode(), 1));
	// - если кода еще нет в списке, он добавляется с частотой 1
	if (!insertInfo.second) {
	// - если код уже в списке (вставка не удалась)
		StatPair &pair = *insertInfo.first;	// Найденная пара с кодом
		++pair.second;	// Инкрементируем частоту найденного кода
	}
	if (ri.critical()) dynamic_cast<RI &>(*this) = ri;
	/* - NB: новая критическая ошибка заменяет старую (чтобы в окончательном
			 сообщении в качестве основной фигурировала последняя, желательно
			 наиболее общая ошибка)	*/
	else upTo(ri);
}


const std::ios_base::openmode modeStreamBinInOut =
		std::ios_base::binary | std::ios_base::in | std::ios_base::out;
// - NB: Указание атрибутов in и out при открытии binary-потоков необходимо

class Input {
public:
	Input() : m_ris(NULL),	m_lastValidPos(-1) {}
	~Input() { close(); }
// - NB: В текущей версии (с полем m_ifs) можно было оставить деструктор
//		 по умолчанию, т.к. файл закрылся бы автоматически
	Input(std::istream *stream, ErrorLogImpl *err = NULL,
		  std::streamsize byteCount = -1);
	Input(const string &fileName, ErrorLogImpl *err);
	void open(std::istream *stream, ErrorLogImpl *err = NULL,
			  std::streamsize byteCount = -1);
	void open(const string &fileName, ErrorLogImpl *err);
	void close();
	void addIssue(RI ri)
		{ if (m_rErr) m_rErr->add(ri); }
	void addIssue(RI::RetCode retCode, uint rcCase = 0)
		{ if (m_rErr) m_rErr->add(RI(retCode, rcCase)); }
// - NB: код функций addIssue надо бы объединить
	ErrorLogImpl *pErrorLog() { return m_rErr; }
	std::streampos getPos() { return m_ris->tellg(); }
		std::streampos lastValidPos() { return m_lastValidPos; }
	bool atEnd();
	// - NB: Функции getPos, atEnd не имеют атрибут const,
	//		 т.к. его не имеет функция m_ris->tellg
	void setPos(std::streampos pos);
	void offset(std::streamoff off);
	void read(void *pBuf, std::streamsize cnt);
	template <class T> void read(T &v);
	void read(std::ostream &os, std::streamsize cnt);
private:
	void check();
	void storeLastValidPos() {
		std::streampos p = m_ris->tellg();
		if (p != std::streampos(-1)) m_lastValidPos = p;
	}
	std::ifstream m_ifs;
	// - используется (через ссылку m_ris) только при чтении из файла
/* - NB: Теперь, когда поток управляется не через ссылку, а через указатель,
		 чтение из файла можно реализовать без специального поля m_ifs
		 (см. класс Output) */
	std::istream *m_ris;
		#ifndef NDEBUG
		std::streampos d_curPos;
		std::streamsize d_size;
		#endif
	std::streampos m_lastValidPos;
	std::streampos m_endPos;
	// - Позиция конца данных или -1, если она определяется концом файла/потока
	ErrorLogImpl *m_rErr;
}; // class Input

Input::Input(std::istream *stream, ErrorLogImpl *err,
			 std::streamsize byteCount) :
	m_ris(stream),
	m_lastValidPos(-1),
	m_rErr(err)
{
	try {
		std::streamoff startPos = m_ris->tellg();
		if (m_ris->fail()) throw RI(RI::ReadErr, 1);
			#ifndef NDEBUG
			d_curPos = m_ris->tellg();
			m_ris->seekg(0, std::ios_base::end);
			d_size = m_ris->tellg() - d_curPos;
			m_ris->seekg(d_curPos);
			#endif
		storeLastValidPos();
		m_endPos = (byteCount == -1) ? -1 : startPos + byteCount;
	}
	catch(RI ri) {
		if (m_rErr) m_rErr->add(ri.toCritical());
	}
}

Input::Input(const string &fileName, ErrorLogImpl *err) :
	m_ris(&m_ifs),
	m_lastValidPos(-1),
	m_rErr(err)
{
	try {
		m_ifs.open(fileName.c_str(), m_ifs.binary);
		if (m_ifs.fail()) throw RI(RI::OpenErr, 1);
			#ifndef NDEBUG
			d_curPos = m_ris->tellg();
			m_ris->seekg(0, std::ios_base::end);
			d_size = m_ris->tellg() - d_curPos;
			m_ris->seekg(d_curPos);
			#endif
		storeLastValidPos();
		m_endPos = -1;
	}
	catch(RI ri) {
		if (m_rErr) m_rErr->add(ri.toCritical());
	}
}

void Input::open(std::istream *stream, ErrorLogImpl *err,
				 std::streamsize byteCount) {
	close();
	m_ris = stream;
	m_rErr = err;
	std::streamoff startPos = m_ris->tellg();
	if (m_ris->fail()) throw RI(RI::ReadErr, 1);
		#ifndef NDEBUG
		d_curPos = m_ris->tellg();
		m_ris->seekg(0, std::ios_base::end);
		d_size = m_ris->tellg() - d_curPos;
		m_ris->seekg(d_curPos);
		#endif
	storeLastValidPos();
	m_endPos = (byteCount == -1) ? -1 : startPos + byteCount;
}

void Input::open(const string &fileName, ErrorLogImpl *err) {
	close();
	m_ris = &m_ifs;
	m_rErr = err;
	m_ifs.open(fileName.c_str(), m_ifs.binary);
	if (m_ifs.fail()) throw RI(RI::OpenErr, 1);
		#ifndef NDEBUG
		d_curPos = m_ris->tellg();
		m_ris->seekg(0, std::ios_base::end);
		d_size = m_ris->tellg() - d_curPos;
		m_ris->seekg(d_curPos);
		#endif
	storeLastValidPos();
	m_endPos = -1;
}

void Input::close() {
	if (!m_ris) return;
	m_rErr = NULL;
	m_lastValidPos = -1;
	std::ifstream *pifs = dynamic_cast<std::ifstream *>(m_ris);
	if (pifs) {
// - NB: В текущей версии (с полем m_ifs) здесь pifs=&m_ifs
		pifs->close();
//		delete pifs;	// - также закрывает файл
// - NB: В текущей версии нельзя выполнять 'delete pifs' (см. выше)
	}
	m_ris = NULL;
}

bool Input::atEnd() {
		#ifndef NDEBUG
		d_curPos = m_ris->tellg();
		#endif
	storeLastValidPos();
	return m_endPos == std::streampos(-1) ? m_ris->peek() == std::char_traits<char>::eof() :
							m_ris->tellg() == m_endPos;
}

void Input::setPos(std::streampos pos) {
	m_ris->seekg(pos);
		#ifndef NDEBUG
		d_curPos = m_ris->tellg();
		#endif
	storeLastValidPos();
}
// - NB: а если pos<0?

void Input::read(void *pBuf, std::streamsize cnt) 
{
	m_ris->read((char *)pBuf, cnt);
	check();

	if (m_ris->gcount() != cnt) 
        throw RI(RI::ReadErr, 2);

	// - проверка "на всякий случай"
	//	 (после вызова check такого не должно быть)
#ifndef NDEBUG
	d_curPos = m_ris->tellg();
#endif

	storeLastValidPos();
}

void Input::read(std::ostream &os, std::streamsize cnt)
{
	copy(os, *m_ris, cnt);
	check();

	if (m_ris->gcount() != cnt)
		throw RI(RI::ReadErr, 3);

	// - проверка "на всякий случай"
	//	 (после вызова check такого не должно быть)
#ifndef NDEBUG
	d_curPos = m_ris->tellg();
#endif

	if (os.fail()) 
        throw RI(RI::WriteErr);

	storeLastValidPos();
}

template <class T> void Input::read(T &v) {
	read((byte *)&v, sizeof v);
}

void Input::offset(std::streamoff off) 
{
	m_ris->seekg(off, m_ris->cur);
	// - NB: Вызов check() здесь не имеет смысла, т.к. оказалось,
	//	 что при  вызове std::istream::seekg() никакие флаги ошибок
	//	 не устанавливаются, а функция tellg после этого просто возвращает
	//	 запрошенное значение позиции (даже если она за пределами потока)
		#ifndef NDEBUG
		d_curPos = m_ris->tellg();
		#endif
	storeLastValidPos();
}

void Input::check() {
	bool eof = m_ris->eof();
	if (!eof) {
		bool fail = m_ris->fail();
		std::streampos pos;
		if (fail || (pos = m_ris->tellg()) == (std::streampos)(-1))
			throw RI(RI::ReadErr, 4);
		// - здесь учитывается также (маловероятный) случай, когда основная
		//	 операция не установила флаг failbit, а следующий за ней вызов
		//	 tellg установил его
		eof = m_endPos != (std::streampos)(-1) && pos > m_endPos;
	}
	if (eof)
		throw RI(RI::DataTrunc);
}


class Output {
public:
	Output() : m_ros(NULL) {}
	Output(std::ostream *stream) : m_ros(NULL) { open(stream); }
	~Output() { close(); }
	void open(std::ostream *stream);
	void open(const string &fileName);
	void close();
	template <class T> void write(const T &v);
	void write(const void *pBuf, std::streamsize cnt);
	void write(std::istream &is, std::streamsize cnt);
private:
	void check();
	std::ostream *m_ros;
		#ifndef NDEBUG
		std::streampos d_curPos;
		#endif
};

void Output::open(std::ostream *stream) {
	close();
	m_ros = stream;
		#ifndef NDEBUG
		d_curPos = m_ros->tellp();
		#endif
}

void Output::open(const string &fileName) {
	close();
	std::ofstream *pofs = new std::ofstream(fileName.c_str(),
											std::ios_base::binary);
	if (pofs->fail()) throw RI(RI::OpenErr, 2);
	m_ros = pofs;
		#ifndef NDEBUG
		d_curPos = m_ros->tellp();
		#endif
}
// - NB: Если все ошибки при выполнении этой функции генерируют исключения,
//		 то возвращаемое RI-значение избыточно

void Output::close() {
	if (!m_ros) return;
	std::ofstream *pofs = dynamic_cast<std::ofstream *>(m_ros);
	if (pofs) {
		pofs->close();
		check();
// - чтобы ошибка при закрытии файла не произошла в деструкторе ниже
//	 (а это действительно важно?)
		delete pofs;	// - NB: деструктор здесь закрывает файл
	}
	m_ros = NULL;
}

void Output::write(const void *pBuf, std::streamsize cnt) {
	m_ros->write((const char *)pBuf, cnt);
	check();
		#ifndef NDEBUG
		d_curPos = m_ros->tellp();
		#endif
}

void Output::write(std::istream &is, std::streamsize cnt) {
	copy(*m_ros, is, cnt);
	if (is.fail()) throw RI(RI::ReadErr, 5);
	check();
		#ifndef NDEBUG
		d_curPos = m_ros->tellp();
		#endif
}

template <class T> void Output::write(const T &v) {
	write((const byte *)&v, sizeof v);
}

void Output::check() {
	if (m_ros->fail()) throw RI(RI::WriteErr);
}


// ============================================================================

namespace CharSets {
const string chars(char chFrom, char chTo) {
	int len = chTo - chFrom + 1;
	if (len <= 0) return "";
	string s;
	s.resize(len);
	for (int n = 0; n < len; ++n) { s[n] = chFrom + n; }
	return s;
}
const string
	upCaseAlpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	loCaseAlpha = "abcdefghijklmnopqrstuvwxyz",
	digits = "0123456789",
	ascii = chars(0, 127);
}

// ============================================================================

//template <typename Num>
//bool inRange(Num val, Num lo, Num hi) {	return val >= lo && val <= hi; }
// - NB: В таком виде шаблон пока не нужен (т.к. типы val и lo/hi обычно разные)
bool inRange(int val, int lo, int hi) {	return val >= lo && val <= hi; }

bool DateTime::valid() const {
	return inRange(year, 1900, 2155) && inRange(timeZone, LST, GMT) &&
		   inRange(month, 1, 12) && inRange(day, 1, 31) &&
		   hour < 24 && min < 60 && sec < 60 && msec < 1000;
}

/*static*/ bool Ident::valid(const string &s) {
	using namespace CharSets;
	static const string charset = chars(33, 96) + chars(123, 126);
	return s.length() < 256 && s.find_first_not_of(charset) == string::npos;
}

/*static*/ bool Units::valid(const string &s) {
	using namespace CharSets;
	static const string charset =
//			loCaseAlpha + upCaseAlpha + digits + " -./()";
				loCaseAlpha + upCaseAlpha + digits + " -./()" + '%';
	// - По примеру Schlumberger допускаем использование символа процента '%'
	//		(вопреки стандарту DLIS 1.0)
	return s.length() < 256 && s.find_first_not_of(charset) == string::npos;
}

// ============================================================================

//template <typename IntType> IntType minValue() {};
//template <typename IntType> IntType maxValue() {};
//template<> unsigned char minValue<unsigned char>() { return 0; }
//template<> unsigned char maxValue<unsigned char>() { return UCHAR_MAX; }
//template<> signed char minValue<signed char>() { return SCHAR_MIN; }
//template<> signed char maxValue<signed char>() { return SCHAR_MAX; }

//template<> unsigned short minValue<unsigned short>() { return 0; }
//template<> unsigned short maxValue<unsigned short>() { return USHRT_MAX; }
//template<> signed short minValue<signed short>() { return SHRT_MIN; }
//template<> signed short maxValue<signed short>() { return SHRT_MAX; }

//template<> unsigned int minValue<unsigned int>() { return 0; }
//template<> unsigned int maxValue<unsigned int>() { return UINT_MAX; }
//template<> signed int minValue<signed int>() { return INT_MIN; }
//template<> signed int maxValue<signed int>() { return INT_MAX; }

//template<> unsigned long minValue<unsigned long>() { return 0; }
//template<> unsigned long maxValue<unsigned long>() { return ULONG_MAX; }
//template<> signed long minValue<signed long>() { return LONG_MIN; }
//template<> signed long maxValue<signed long>() { return LONG_MAX; }

namespace Representation {

bool valid(Code code) {	return code > Undefined && code < Count; }

//template <class T>
//Code getProperCode() {/*...*/}
//template<> Code getProperCode<int>()
//  { return (sizeof(int) == 2) ? SNORM : SLONG; }
//template<> Code getProperCode<double>() { return FDOUBL; }
//// ...

// Точное соответствие между типами C++ и кодами Representation::Code
template <typename T>
inline Code typeCode() { return Undefined; }
template<> inline Code typeCode<byte>()		  { return USHORT; }
template<> inline Code typeCode<int8>()		  { return SSHORT; }
template<> inline Code typeCode<uint16>()	  { return UNORM;  }
template<> inline Code typeCode<int16>()	  { return SNORM;  }
template<> inline Code typeCode<uint32>()	  { return ULONG;  }
template<> inline Code typeCode<ieeeSingle>() { return FSINGL; }
template<> inline Code typeCode<ieeeDouble>() { return FDOUBL; }
template <typename T>
inline Code typeCode(const T &) { return typeCode<T>(); }

// Оптимальное соответствие между типами C++ и кодами Representation::Code
template <typename T>
inline Code typeBestCode() { return typeCode<T>(); }
template<> inline Code typeBestCode<string>()	  { return ASCII;  }
template<> inline Code typeBestCode<bool>()		  { return STATUS; }
template<> inline Code typeBestCode<Ident>()	  { return IDENT;  }
template<> inline Code typeBestCode<Units>()	  { return UNITS;  }
template<> inline Code typeBestCode<ObjectName>() { return OBNAME; }
template<> inline Code typeBestCode<DateTime>()	  { return DTIME;  }
//template <typename T>
//inline Code typeBestCode(const T &) { return typeBestCode<T>(); }
//template <typename T>
//inline Code typeBestCode(const vector<T> &) { return typeBestCode<T>(); }

unsigned getSize(Code code) {
	static uint sizes[Count];
	static bool ready = false;
	if (!ready) {
		for (int nCode = 0; nCode < Count; ++nCode) {
			uint sz;
			switch ((Code)nCode) {
			case SSHORT: case USHORT: case STATUS:
				sz = 1; break;
			case FSHORT: case SNORM:  case UNORM:
				sz = 2; break;
			case FSINGL: case ISINGL: case VSINGL: case SLONG: case ULONG:
				sz = 4; break;
			case FSING1: case FDOUBL: case CSINGL: case DTIME:
				sz = 8; break;
			case FSING2:
				sz = 12; break;
			case FDOUB1: case CDOUBL:
				sz = 16; break;
			case FDOUB2:
				sz = 24; break;
			default:
				sz = -1;  // - переменная длина
			}
			sizes[nCode] = sz;
		}
		ready = true;
	}
	return sizes[code];
}
//RI toDlis(const int &value, Code code, byte *ptrDlisValue) {/*...*/}
//RI fromDlis(int &value, Code code, byte *ptrDlisValue) {/*...*/}
// ...
}; // namespace Representation

// Класс  предназначен для определения возможности помещения целых значений
//	одного типа в целочисленные поля другого типа
//	(класс не привязан к модулю Dlis)
class IntTypes {
public:
	template <typename IntType, typename IntValueType>
	static bool canHold(IntValueType value) {
		return IntField::intField<IntType>().canHold(value);
	}
	// - Возвращает true, если тип IntType может содержать значение value;
	//	 тип IntType передается как явный аргумент шаблона
// - пока используется только в тесте
	template <typename IntType, typename IntValueType>
	static bool canHold(const IntType &, IntValueType value) {
		return IntField::intField<IntType>().canHold(value);
	}
	// - Возвращает true, если тип IntType может содержать значение value;
	//	 тип IntType передается как тип первого параметра функции
	template <typename IntType, typename OtherIntType>
	static bool canHold() {
		const IntField &other(IntField::intField<OtherIntType>());
		return IntField::intField<IntType>().canHold(other);
	}
	// - Возвращает true, если тип IntType может содержать любые значения
	//	 типа OtherIntType
// - пока используется только в тесте
protected:
	class IntField {	// Описатель представления целого типа или значения
	public:
		enum BitCount {bc7, bc8, bc15, bc16, bc31, bc32};
		// - (bc7, bс15 и bс31 используются только для положительных чисел)
		template <typename IntType>
			static const IntField &intField();
		// - возвращает описатель типа
		template<> static const IntField &intField<byte>() {
			static IntField t(bc8, false); return t;
		}
		template<> static const IntField &intField<int8>() {
			static IntField t(bc8, true); return t;
		}
		template<> static const IntField &intField<uint16>() {
			static IntField t(bc16, false); return t;
		}
		template<> static const IntField &intField<int16>() {
			static IntField t(bc16, true); return t;
		}
		template<> static const IntField &intField<uint32>() {
			static IntField t(bc32, false); return t;
		}
		template<> static const IntField &intField<int32>() {
			static IntField t(bc32, true); return t;
		}
		template <typename IntType>
		static void getValueInfo(IntType value, BitCount &bc, bool &negative) {
			IntField f = intField(value);
			bc = f.bitCount;
			negative = f.isSigned;
		}
		bool canHold(const IntField &other) const;
		template <typename IntType>
		bool canHold(IntType value) const { return canHold(intField(value)); }
		/* - NB: Хотя эта операция не быстрая (см., например, код функций
				 getBitCount), предваряющий вызов canHold(intField<IntType>())
				 (при условии, что value будет передаваться по ссылке,
				 а не по значению) вряд ли значительно ускорит ее -
				 см., например, код canHold(const IntField &) */
	private:
//		enum BitCount {bc7, bc8, bc15, bc16, bc31, bc32};
		static BitCount getBitCount(uint32 value);
		static BitCount getBitCount(int32 value);
		template <typename IntType>
			static IntField intField(IntType value);
		// - возвращает описатель значения
/* - NB: Если вынести intField из private-секции, в классе-наследнике
		 DlisIntTypes могут возникать неоднозначности из-за его одноименной
		 функции */
	// ------------------------------------------------------------------------
		IntField(BitCount bitCount, bool isSigned) :
			bitCount(bitCount), isSigned(isSigned) {}
	// ------------------------------------------------------------------------
		const BitCount bitCount;
		const bool isSigned;
	}; // struct IntField
}; // struct IntTypes

template <typename IntType>
/*static*/ IntTypes::IntField IntTypes::IntField::intField(IntType value) {
	IntField::BitCount bitCount;
	bool isSigned;
	if (intField<IntType>().isSigned) {	// знаковый тип
		isSigned = value < 0;
		if (isSigned) bitCount = getBitCount((int32)value);
		else		  bitCount = getBitCount((uint32)value);
	} else {							// беззнаковый тип
		isSigned = false;
		bitCount = getBitCount((uint32)value);
	}
	return IntField(bitCount, isSigned);
}

/*static*/
IntTypes::IntField::BitCount IntTypes::IntField::getBitCount(uint32 value) {
	if		(value < 0x00000080) return IntField::bc7;
	else if (value < 0x00000100) return IntField::bc8;
	else if (value < 0x00008000) return IntField::bc15;
	else if (value < 0x00010000) return IntField::bc16;
	else if (value < 0x80000000) return IntField::bc31;
	else return IntField::bc32;
}

/*static*/
IntTypes::IntField::BitCount IntTypes::IntField::getBitCount(int32 value) {
	if		(value >= -0x80) return IntField::bc8;
	else if (value >= -0x8000) return IntField::bc16;
	else return IntField::bc32;
}

bool IntTypes::IntField::canHold(const IntField &other) const {
	if (!isSigned && other.isSigned)
		return false;
	else if (bitCount == other.bitCount)
		return isSigned == other.isSigned;
	else
		return bitCount > other.bitCount;
}

// ============================================================================

class DlisIntTypes : private IntTypes {
// - NB: Для классов, содержащих только статические функции,
//		 public-наследование избыточно (а на практике иногда вызывало путаницу)
	typedef Representation::Code RCode;
public:
	struct OtherTypes {	// Класс-заглушка для значений любого (нецелого) типа
		template <typename T>
		static RCode valueBestCode(const T &val) {
			return Representation::Undefined;
		}
	};
	struct Integers {	// Класс поддержки значений целого типа
		template <typename T>
		static RCode valueBestCode(const T &val);
	};
	template <typename T> struct Type : OtherTypes {};
	template<> struct Type<byte>   : Integers {};
	template<> struct Type<int8>   : Integers {};
	template<> struct Type<uint16> : Integers {};
	template<> struct Type<int16>  : Integers {};
	template<> struct Type<uint32> : Integers {};
	template<> struct Type<int32>  : Integers {};
// ----------------------------------------------------------------------------
	template <typename IntType>
	static bool canHoldCode(const IntType &, RCode intCode) {
//		return IntField::intField<IntType>().canHold(intField(intCode));

		const IntField &f = IntField::intField<IntType>();
		if (f.canHold(intField(intCode)))
			return true;
		else if (intCode == Representation::UVARI) {
//			return &f == &IntField::intfield<int32>();
// - NB: Ошибка компиляции по непонятным причинам
//		 (хотя ее нет в похожей ситуации в функции canHoldType)
				const IntField &fi32 = IntField::intField<int32>();
				return &f == &fi32;
			// - Тип int32 тоже может содержать все значения кода UVARI
		}
	}
	// - Возвращает true, если тип IntType может содержать любое значение
	//	 в (целочисленном) коде представления intCode;
	//	 тип IntType передается как тип первого параметра функции
	template <typename IntType>
	static bool canHoldType(RCode intCode, const IntType &) {
//		return intField(intCode).canHold(IntField::intField<IntType>());

		const IntField &f = IntField::intField<IntType>();
		bool ok = intField(intCode).canHold(f);
		if (ok) {
			if (intCode == Representation::UVARI)
				ok = &f != &IntField::intField<uint32>();
				// - Код UVARI не может содержать все значения uint32
		}
		return ok;
	}
	// - Возвращает true, если код представления intCode может содержать любое
	//	 значение типа IntType;
	//	 тип IntType передается как тип второго параметра функции
	template <typename IntValueType>
	static bool canHoldValue(RCode intCode, IntValueType value) {
//		return intField(intCode).canHold(value);
		bool ok = intField(intCode).canHold(value);
		if (ok) {
			if (intCode == Representation::UVARI)
				ok = value <= Representation::maxUVARI;
		}
		return ok;
	}
	// - Возвращает true, если код представления intCode может содержать
	//	 значение value;
private:
	static const IntField &intField(RCode code);
};

template <typename T>
/*static*/ Representation::Code
DlisIntTypes::Integers::valueBestCode(const T &val) {
	using namespace Representation;
	IntField::BitCount bc;
	bool negative;
	IntField::getValueInfo(val, bc, negative);
	if (negative)	// - если число отрицательное
		switch (bc) {
		case IntField::bc8:  return SSHORT;
		case IntField::bc16: return SNORM;
		case IntField::bc32: return SLONG;
			default: assert(false);
		}
	else
		if (val <= Representation::maxUVARI)
			return UVARI; // - лучший код для одиночных неотрицательных чисел
		else
			return ULONG;
}

/*static*/ const DlisIntTypes::IntField &DlisIntTypes::intField(RCode code) {
	using namespace Representation;
	switch (code) {
	case USHORT: return IntField::intField<byte>();
	case SSHORT: return IntField::intField<int8>();
	case UNORM:  return IntField::intField<uint16>();
	case SNORM:  return IntField::intField<int16>();
	case UVARI:
	case ULONG:  return IntField::intField<uint32>();
	case SLONG:  return IntField::intField<int32>();
	}
}
// - NB: Нужно реализовать через статический массив

// ============================================================================

const bool d_BatchValRead = true;

class Value {	// Контейнер значений во внутреннем формате DLIS
public:
	typedef Representation::Code Code;	// - для краткости
//	bool isEmpty() const { return m_raw.size() == 0; }
	class Convertor;
	static void skip(const byte *&p, Code code);
	bool empty() const { return m_raw.size() == 0; }
	void clear() { m_raw.clear(); }
protected:
	static void readBytes(byte *p, Input &input_file, size_t count) {
		input_file.read(p, count);
	}
	static void readBytes(byte *p, const byte *&pin, size_t count) {
		memcpy(p, pin, count);
		pin += count;
	}
	static void writeBytes(Output &out, const byte *p, size_t count) {
		out.write(p, count);
	}
	static void writeBytes(Value &rawVal, const byte *p, size_t count) {
		rawVal.readBytesNext(p, count);
	}
	const byte *cfirstBytePtr() const
		{ return empty() ? NULL : &m_raw.front(); }
	byte *firstBytePtr()
		{ return const_cast<byte *>(cfirstBytePtr()); }
	template <typename In>
	void readBytesNext(In &input_file, size_t count) {
		if (count == 0) return;
		// - иначе индекс endpos выйдет за пределы контейнера m_raw
		size_t endpos = m_raw.size();
		extendBy(count);
		readBytes(&m_raw[endpos], input_file, count);
	}
	template <class Out>
	static void writeBytesNext(Out &out, const byte *&p, size_t count) {
		writeBytes(out, p, count);
		p += count;
	}
	template <typename In>
		void readNext(In &input_file, Code code, size_t count = 1);
	template <class Out>
		void writeNext(Out &out, Code code, size_t count = 1);
	template <class Out>
		static void writeNext(Out &out, const byte *&p,
							  Code code, size_t count = 1);
// - NB: Функции readNext и writeNext используются рекурсивно
//	template <typename T>
//		static RI getNext(T &var, const byte *&p,
//						  Code code, long index);
//	template <class T>
//		static RI getNext(vector<T> &vec, const byte *&p,
//						  Code code, size_t count);
	Bytes m_raw;
private:
	template <typename InType, void (Value:: *process)(InType &, Code)>
		void processCompound(InType &input_file, Code code);
// - NB: Этот template нельзя использовать с функцией skip, т.к. она статическая
//	void readCompound(Input &in, Code code)
//		{ processCompound<Input, readNext>(in, code); }
//	void readNext(Input &in, Code code);
//	// - NB: readNext() вызывается (только) из read() рекурсивно
	void extendBy(size_t count) {
		m_raw.resize(m_raw.size() + count);
	}
}; // class Value

class Value::Convertor {
private:
	static const int32 maxPreciseIntAsFloat = 1/FLT_EPSILON + 0.5;
// - (примерное?) значение максимального целого числа, которое можно хранить
//	 в формате float без потери точности
	typedef Representation::Code Code;	// - для удобства
public:
// Определение длины UVARI-формата числа по первому байту формата
	static uint getSizeUVARI(byte firstByte) 
    {
		switch (firstByte >> 6) {	// Определяем длину по старшим 2 битам
			case 2:  return 2;	// '10'
			case 3:  return 4;	// '11'
			default: return 1;	// '00' или '01'
		}
	}
// Определение длины UVARI-формата числа по значению числа
	static uint getSizeUVARIof(uint32 val) {
		if		(val <= 0x7F)	return 1;
		else if (val <= 0x3FFF) return 2;
		else					return 4;
	}
// Конвертирование целых типов
	static RI fromRaw(byte &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	static RI fromRaw(int8 &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	static RI fromRaw(uint16 &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	static RI fromRaw(int16 &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	static RI fromRaw(uint32 &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	static RI fromRaw(int32 &val, const byte *&p, Code c) {
		return fromRawIntDisp(val, p, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const byte &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const int8 &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const uint16 &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const int16 &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const uint32 &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <class Out>
	static RI toRaw(Out &out, const int32 &val, Code c) {
		return toRawIntDisp(out, val, c);
	}
	template <typename IntType>
	static RI fromRawIntDisp(IntType &val, const byte *&p, Code c, bool test = false)
    {
		using namespace Representation;
		switch (c) 
        {
		    case USHORT: 
                return fromRawInt<byte>(val, p, test);

		    case SSHORT:    
                return fromRawInt<int8>(val, p, test);

		    case UNORM:  
                return fromRawInt<uint16>(val, p, test);

		    case SNORM:  
                return fromRawInt<int16>(val, p, test);

		    case ULONG:
		    case UVARI:  
                return fromRawInt<uint32>(val, p, test, c);

		    case SLONG:  
                return fromRawInt<int32>(val, p, test);

		    default: 
                return RI(RI::ProgErr, 12).toCritical();
		}
	}

// Конвертирование вещественных типов
	static RI fromRaw(ieeeSingle &val, const byte *&p, Code c) 
    {
		using namespace Representation;

		switch (c) 
        {
		case FSINGL:
			fromRaw(val, p);
			break;

		case FDOUBL: 
        {
			ieeeDouble d;
			fromRaw(d, p);
			if (std::abs(d) < FLT_MIN || abs(d) > FLT_MAX)
				return RI(RI::FromRawValErr, 1).toCritical();
			val = d;
			return RI(RI::FromRawPrec);
		}

		case SSHORT: 
        case SNORM: 
        case SLONG: 
        {
			int32 i;
			fromRawIntDisp(i, p, c);
			val = i;
			if (abs(i) > maxPreciseIntAsFloat) return RI(RI::FromRawPrec);
			// - NB: в предположении, что ieeeSingle=float
			break;
		}

		case USHORT: 
        case UNORM: 
        case ULONG: 
        case UVARI: 
        {
			uint32 i;
			fromRawIntDisp(i, p, c);
			val = i;
			if (i > maxPreciseIntAsFloat) return RI(RI::FromRawPrec);
			// - NB: в предположении, что ieeeSingle=float
			break;
		}

		default:
			return RI(RI::FromRawNone, 1).toCritical();
		}
		return RI();
	}

	static RI fromRaw(ieeeDouble &val, const byte *&p, Code c) 
    {
		using namespace Representation;
		switch (c) 
        {
		    case /*Representation::*/FSINGL: 
                {
		        	ieeeSingle s;
		        	fromRaw(s, p);
		        	val = s;
		        	break;
		        }

		    case /*Representation::*/FDOUBL:
		    	fromRaw(val, p);
		    	break;

		    case SSHORT: 
            case SNORM: 
            case SLONG: 
                {
		        	int32 i;

		        	fromRawIntDisp(i, p, c);
		        	val = i;
		        	break;
		        }

		    case USHORT: 
            case UNORM: 
            case ULONG: 
            case UVARI:
                {
		        	uint32 i;
		        	fromRawIntDisp(i, p, c);
		        	val = i;
		        	break;
		        }

		    default:
		    	return RI(RI::FromRawNone, 2).toCritical();

		}
		return RI();
	}
// - NB: Функции fromRaw конвертирования вещественных типов надо бы объединить
	template <class Out>
	static RI toRaw(Out &out, ieeeDouble val, Code c) {
		using namespace Representation;
		switch (c) {
		case FSINGL: toRaw(out, ieeeSingle(val)); return RI(RI::FromRawPrec);
		case FDOUBL: toRaw(out, val);			  return RI();
		default: return RI(RI::FromRawNone, 8).toCritical();
		}
	}
	template <class Out>
	static RI toRaw(Out &out, ieeeSingle val, Code c) {
		using namespace Representation;
		switch (c) {
		case FSINGL: toRaw(out, val);			  return RI();
		case FDOUBL: toRaw(out, ieeeDouble(val)); return RI();
		default: return RI(RI::FromRawNone, 8).toCritical();
		}
	}
// Конвертирование для типа Representation::Code
	static RI fromRaw(Representation::Code &val, const byte *&p,
					  Code c = Representation::USHORT) {
		using namespace Representation;
		if (c != USHORT) return RI(RI::FromRawNone, 3).toCritical();
		val = (Code)*p++;
		if (val < FSHORT || val >= Count) return RI(RI::BadRaw, 1).toCritical();
		return RI();
	}
	template <class Out>
	static RI toRaw(Out &out, Representation::Code val,
					Code c = Representation::USHORT) {
		using namespace Representation;
		if (c != USHORT) return RI(RI::FromRawNone, 9).toCritical();
		if (val < FSHORT || val >= Count) return RI(RI::BadRaw, 3).toCritical();
// - NB: Возможно, код ошибки BadRaw здесь не совсем подходит
		toRaw(out, (byte)val);
		return RI();
	}
// Конвертирование строковых Representation-кодов
	static RI fromRaw(string &val, const byte *&p, Code c);
/* - NB: Тело функции вынесено за пределы класса, т.к. должно находиться после
		 тела специализации шаблона fromRawCode(), который в ней используется
		 (иначе возникает ошибка 'specialization of ... after instantiation') */
	template <class Out>
		static RI toRaw(Out &out, const string &val, Code c);
	static RI fromRaw(Symbol &val, const byte *&p, Code c) {
		string s;
		RI ri = fromRaw(s, p, c);
		if (ri.ok()) val.set(s);
		return ri;
	}
	static RI fromRaw(ObjectName &val, const byte *&p, Code c) {
		using namespace Representation;
		if (c != OBNAME) return RI(RI::FromRawNone, 4).toCritical();
		uint32 ori;
		fromRaw(ori, p, UVARI);
		byte cpy;
		fromRaw(cpy, p);
		string id;
		fromRaw(id, p, IDENT);
		val.set(Ident(id), ori, cpy);
		return RI();
// - NB: Нужно бы еще проверить на соответствие стандарту DLIS
	}
	template <class Out>
	static RI toRaw(Out &out, const Symbol &val, Code c) {
		return toRaw(out, val.str(), c);
	}
	template <class Out>
	static RI toRaw(Out &out, const ObjectName &val, Code c) {
		using namespace Representation;
		if (c != OBNAME) return RI(RI::ToRawNone, 2).toCritical();
		toRaw(out, val.originId(), UVARI);
		toRaw(out, (byte)val.copyNum());
// - NB: Явно указываем тип byte на случай изменения определения функции
//		 ObjectName::copyNum()
		toRaw(out, val.ident(), IDENT);
/* - NB: Не используем возвращаемый код в предположении, что длина возвращаемой
		 функцией ObjectName::ident строки (как экземпляра Ident)
		 гарантированно не длиннее 255 символов */
		return RI();
	}
// Конвертирование для кода Representation::DTIME
	static RI fromRaw(DateTime &val, const byte *&p, Code c) {
		if (c != Representation::DTIME)
			return RI(RI::FromRawNone, 10).toCritical();
		DateTime dt;
		byte yr1900;
		byte tz_m;	// 2-й байт, содержащий timeZone и month
		uint16 ms;
		fromRaw(yr1900, p);
		fromRaw(tz_m, p);
		fromRaw(dt.day, p);
		fromRaw(dt.hour, p);
		fromRaw(dt.min, p);
		fromRaw(dt.sec, p);
		fromRaw(ms, p);
		dt.year = 1900 + yr1900;
		dt.timeZone = DateTime::TimeZone(tz_m >> 4); // - старшие 4 бита tz_m
		dt.month = tz_m & 0xf;						 // - младшие 4 бита tz_m
		if (littleEndian) reverseBytes(ms);
		dt.msec = ms;
		if (!dt.valid()) return RI(RI::BadRaw, 4).toCritical();
		val = dt;
		return RI();
	}
	template <class Out>
	static RI toRaw(Out &out, const DateTime &val, Code c) {
		if (c != Representation::DTIME)
			return RI(RI::ToRawNone, 3).toCritical();
		if (!val.valid()) return RI(RI::BadDTime).toCritical();
		byte yr1900 = val.year - 1900;
		byte tz_m = val.month + (val.timeZone << 4);
		uint16 ms = val.msec;
		if (littleEndian) reverseBytes(ms);
		toRaw(out, yr1900);
		toRaw(out, tz_m);
		toRaw(out, val.day);
		toRaw(out, val.hour);
		toRaw(out, val.min);
		toRaw(out, val.sec);
		toRaw(out, ms);
		return RI();
	}
// Конвертирование для кода Representation::STATUS
	static RI fromRaw(bool &val, const byte *&p, Code c) {
		if (c != Representation::STATUS)
			return RI(RI::FromRawNone, 5).toCritical();
// - NB: Здесь можно было бы также реализовать чтение значений 0 и 1,
//		 представленных в целых Representation-кодах
		byte b = *p++;
		if (b > 1) return RI(RI::BadRaw, 2).toCritical();
		val = (b == 1);
		return RI();
	}
	template <class Out>
	static RI toRaw(Out &out, bool val, Code c) {
		if (c != Representation::STATUS)
			return RI(RI::ToRawNone, 1).toCritical();
		toRaw(out, (byte)(val ? 1 : 0));
		return RI();
	}
private:
// Шаблон при точном соответствии типов
//	 (для целых и вещественных Representation-кодов)
	template <typename T>
	void static fromRaw(T &v, const byte *&p) {
		uint sz = sizeof v;
		if (sz == 1)
			reinterpret_cast<byte &>(v) = *p++;
		else {
			copy(v, p);
			p += sz;
			if (littleEndian) reverseBytes(v);
		}
	}
	template <class Out, typename T>
	void static toRaw(Out &out, const T &v) {
		uint sz = sizeof v;
		if (sz == 1)
			writeBytes(out, (byte *)&v, 1);
		else {
			T vcopy = v;
			if (littleEndian) reverseBytes(vcopy);
			writeBytes(out, (byte *)&vcopy, sz);
		}
	}
// Шаблон при статически заданном Representation-коде
	template <Representation::Code c, typename T>
	static RI fromRawCode(T &val, const byte *&p) {
			return RI(RI::ProgErr, 2).toCritical();
	}
	// - NB: Для конкретизации шаблона fromRawCode в коде fromRawInt нужно
	//		 определить ее код в общем случае (хотя реально он не вызывается)
/* - NB: Есть только одна специализация шаблона fromRawCode по первому
		 аргументу(Representation::UVARI), и ее можно просто выделить
		 в отдельную функцию и назвать, например, fromUVARI (как для шаблона
		 toRawCode была выделена функция toUVARI) */
	template <class Out>
		static RI toUVARI(Out &out, const uint32 &val);
// Шаблоны для целых типов
//	 (CodeIntType - тип, соответствующий значению параметра c по умолчанию)
//---------------------------
//	template <typename CodeIntType, typename CustomIntType>
//	static RI fromRawInt(CustomIntType &val, const byte *&p,
//						 Code c = Representation::typeCode<CodeIntType>()) {
//	// - предполагается, что тип CodeIntType "шире" типа CustomIntType)
//		using namespace Representation;
//		CodeIntType inVal;
//		RI ri;
//		if (c == typeCode<CodeIntType>())
//			fromRaw(inVal, p);		// - точное соответствие типов
//		else if (c == UVARI) {
//			ri = fromRawCode<UVARI>(inVal, p);	// - особый случай
//			if (!ri.ok()) return ri;
//		}
//			else return RI(RI::ProgErr, 3);
////		bool outOfRange = inVal > maxValue<CustomIntType>() ||
////						  inVal < minValue<CustomIntType>();
////		if (outOfRange) return RI(RI::FromRawErr, 2);
//			if (!IntTypes::canHold<CustomIntType>(inVal))
//				return RI(RI::FromRawErr, 2).toCritical();
//		val = inVal;
//		return RI();
//	}
//---------------------------
	template <typename CodeIntType, typename CustomIntType>
	static RI fromRawInt(CustomIntType &val, const byte *&p, bool test,
						 Code c = Representation::typeCode<CodeIntType>()) 
    {
		using namespace Representation;

		CodeIntType     inVal;

		if (c == typeCode<CodeIntType>())
			fromRaw(inVal, p);		                // - точное соответствие типов
		else if (c == UVARI)
			fromRawCode<UVARI>(inVal, p);	        // - особый случай
		else 
            return RI(RI::ProgErr, 3).toCritical();

		if (test) 
        {
			if (!IntTypes::canHold<CustomIntType>(inVal))
				return RI(RI::FromRawValErr, 2).toCritical();
		}
		val = inVal;
		return RI();
	}
//---------------------------
	template <typename CodeIntType, class Out, typename CustomIntType>
	static RI toRawInt(Out &out, const CustomIntType &val,
					   Code c = Representation::typeCode<CodeIntType>()) {
		using namespace Representation;
//		bool outOfRange = val > maxValue<CodeIntType>() ||
//						  val < minValue<CodeIntType>();
//		if (outOfRange) return RI(RI::FromRawErr, 3);
		CodeIntType outVal = val;
		if (c == typeCode<CodeIntType>()) {
			toRaw(out, outVal);		// - точное соответствие типов
			return RI();
		}
		else if (c == UVARI)
			return toUVARI(out, outVal);	// - особый случай
			else return RI(RI::ProgErr, 4).toCritical();
	}
// Шаблоны-"диспетчеры" Representation-кодов для целых типов
//---------------------------
//	template <typename CustomIntType>
//	static RI fromRawIntDisp(CustomIntType &val, const byte *&p, Code c) {
//		using namespace Representation;
//		switch (c) {
//		case USHORT: return fromRawInt<byte>(val, p);
//		case SSHORT: return fromRawInt<int8>(val, p);
//		case UNORM:  return fromRawInt<uint16>(val, p);
//		case SNORM:  return fromRawInt<int16>(val, p);
//		case ULONG:
//		case UVARI:  return fromRawInt<uint32>(val, p, c);
//		case SLONG:  return fromRawInt<int32>(val, p);
//			default: return RI(RI::ProgErr, 12).toCritical();
//		}
//	}
//---------------------------
//	template <typename IntType>
//	static RI fromRawIntDisp(IntType &val, const byte *&p, Code c,
//							 bool test = false) {
//		using namespace Representation;
//		switch (c) {
//		case USHORT: return fromRawInt<byte>(val, p, test);
//		case SSHORT: return fromRawInt<int8>(val, p, test);
//		case UNORM:  return fromRawInt<uint16>(val, p, test);
//		case SNORM:  return fromRawInt<int16>(val, p, test);
//		case ULONG:
//		case UVARI:  return fromRawInt<uint32>(val, p, test, c);
//		case SLONG:  return fromRawInt<int32>(val, p, test);
//			default: return RI(RI::ProgErr, 12).toCritical();
//		}
//	}
// - функция fromRawIntDisp перенесена в public-секцию (т.к. она используется
//	 в SingleValue::fromRaw)
//---------------------------
	template <class Out, typename CustomIntType>
	static RI toRawIntDisp(Out &out, const CustomIntType &val, Code c) {
		using namespace Representation;
		switch (c) {
		case USHORT: return toRawInt<byte>(out, val);
		case SSHORT: return toRawInt<int8>(out, val);
		case UNORM:  return toRawInt<uint16>(out, val);
		case SNORM:  return toRawInt<int16>(out, val);
		case ULONG:
		case UVARI:  return toRawInt<uint32>(out, val, c);
		case SLONG:  return toRawInt<int32>(out, val);
		case FSINGL:
			if (abs((double)val) > maxPreciseIntAsFloat)
				return RI(RI::ToRawPrec);
/* - NB: Приведение val к типу double пришлось применить из-за того, что
		 конкретизация шаблона для CustomIntType=uint32 приводила к ошибке
		 компилятора "'abs' : ambiguous call to overloaded function" */
			toRaw(out, (ieeeSingle)val);
			return RI();
		case FDOUBL:
			toRaw(out, (ieeeDouble)val);
			return RI();
			default: return RI(RI::ProgErr, 13).toCritical();
		}
	}
}; // class Value::Convertor

// Специализации при статически заданном Representation-коде
template<>
/*static*/ RI Value::Convertor::fromRawCode<Representation::UVARI>
				(uint32 &val, const byte *&p) {
	uint len = getSizeUVARI(*p);	// Длина (число байт) представления UVARI
	switch (len) {
	case 1:
		val = *p++;
		break;
	case 2: {
		uint16 v16;
		memcpy(&v16, p, len);
		p += len;
		if (littleEndian) reverseBytes(v16);
		v16 &= 0x3fff;		// Обнуляем старшие 2 бита, указывавшие длину
		val = v16;
		break;
	}
	case 4:
		memcpy(&val, p, len);
		p += len;
		if (littleEndian) reverseBytes(val);
		val &= 0x3fffffff;	// Обнуляем старшие 2 бита, указывавшие длину
		break;
	}
	return RI();
}

template<class Out>
///*static*/ RI Value::Convertor::toUVARI(Out &out, uint32 val) {
// - NB: такое объявление параметра val компилятор не принимает - ??????????
//		 ("'...::toRawCode': cannot be explicitly specialized")
/*static*/ RI Value::Convertor::toUVARI(Out &out, const uint32 &val) {
	uint len = getSizeUVARIof(val);	// Длина (число байт) представления UVARI
	switch (len) {
	case 1:
		toRaw(out, val, Representation::USHORT);
		break;
	case 2: {
		uint16 v16 = val;
		v16 |= 0x8000;		// Устанавливаем старший бит 2-байтового числа
		toRaw(out, v16);
		// - NB: Изменение порядка байт (вызов reverseBytes) выполняется в toRaw
		break;
	}
	case 4: {
		uint32 v32 = val;
		v32 |= 0xC0000000;	// Устанавливаем 2 старших бита 4-байтового числа
		toRaw(out, v32);
		break;
	}
	}
	return RI();
}

// Специализации общего шаблона для строковых Representation-кодов
/*static*/ RI Value::Convertor::fromRaw(string &val, const byte *&p, Code c) {
	using namespace Representation;
	uint32 len;
	switch (c) {
	case IDENT:
	case UNITS: len = *p++; break;
	case ASCII:	fromRawCode<UVARI>(len, p); break;
	default: return RI(RI::FromRawNone, 6).toCritical();
	}
	val.assign((char *)p, len);
	p += len;
	return RI();
}
template <class Out>
/*static*/ RI Value::Convertor::toRaw(Out &out, const string &val, Code c) {
	using namespace Representation;
	uint32 len = val.length();
	switch (c) {
	case IDENT:	case UNITS:
		if (len > 0xFF) return RI(RI::ToRawValErr, 1).toCritical();
		toRaw(out, len, USHORT);
		break;
	case ASCII:
		toUVARI(out, len);
		break;
	default:
		return RI(RI::FromRawNone, 6).toCritical();
	}
	writeBytes(out, (byte *)val.data(), len);
	return RI();
}
// - NB: Здесь можно было бы также реализовать чтение и запись целых
//		 и вещественных чисел в строковом представлении
// end of class Value::Convertor definitions

/*static*/ void Value::skip(const byte *&p, Code code) {
	using namespace Representation;
	uint size = getSize(code);
	if (size != -1)
		p += size;
	else {	// Значение переменной длины
		switch (code) {
		case UVARI: case ORIGIN: {
			p += Convertor::getSizeUVARI(*p);
			break;
		}
		case ASCII:
		case IDENT:	case UNITS: {
			uint32 strLen;
			if (code == ASCII) Convertor::fromRaw(strLen, p, UVARI);
			// - NB: см. примечание в коде readNext
			else strLen = *p++;
			p += strLen;
			break;
		}
		case OBNAME:
			skip(p, ORIGIN);
			skip(p, USHORT);
			skip(p, IDENT);
			break;
		case OBJREF:
			skip(p, IDENT);
			skip(p, OBNAME);
			break;
		case ATTREF:
			skip(p, IDENT);
			skip(p, OBNAME);
			skip(p, IDENT);
			break;
		} // switch (code)
// - NB: Т.к. функция статическая, шаблон processCompound использовать нельзя
	}
}

template <typename InType,
		  void (Value:: *process)(InType &, Representation::Code)>
void Value::processCompound(InType &input_file, Code code) {
	using namespace Representation;
	switch (code) {
	case OBNAME:
		(this->*process)(input_file, ORIGIN);
		(this->*process)(input_file, USHORT);
		(this->*process)(input_file, IDENT);
		break;
	case OBJREF:
		(this->*process)(input_file, IDENT);
		(this->*process)(input_file, OBNAME);
		break;
	case ATTREF:
		(this->*process)(input_file, IDENT);
		(this->*process)(input_file, OBNAME);
		(this->*process)(input_file, IDENT);
		break;
	}
}

template <typename In>
void Value::readNext(In &input_file, Code code, size_t count) 
{
	using namespace Representation;
	if (count == 0) return;
	size_t pos = m_raw.size();
	// - Запоминаем текущее положение, т.к. далее оно будет изменено
	//	 в прямых и косвенных (через readNext) вызовах функции readBytesNext
	uint elsize = getSize(code);
	if (elsize != -1) {
		size_t size = elsize * count;
		readBytesNext(input_file, size);
	}
	else {	// Значение переменной длины
		if (count > 1)
			for (uint32 n = 0; n < count; ++n) readNext(input_file, code);
		else {
			switch (code) {
			case UVARI: case ORIGIN: {
				readBytesNext(input_file, 1);		// Первый байт кода UVARI
				uint len = Convertor::getSizeUVARI(m_raw[pos]);
				if (len > 1) readBytesNext(input_file, len - 1);
				break;
			}
			case ASCII:
			case IDENT:	case UNITS: {
				uint32 strLen;
				if (code == ASCII) {
					readNext(input_file, UVARI);
					const byte *p = &m_raw[pos];
					Convertor::fromRaw(strLen, p, UVARI);
					// - NB: Не проверяем код возврата, т.к. используется uint32
				}
				else {
					readNext(input_file, USHORT);
					strLen = m_raw[pos];
				}
				readBytesNext(input_file, strLen);
				break;
			}
			case OBNAME:
				readNext(input_file, ORIGIN);
				readNext(input_file, USHORT);
				readNext(input_file, IDENT);
				break;
			case OBJREF:
				readNext(input_file, IDENT);
				readNext(input_file, OBNAME);
				break;
			case ATTREF:
				readNext(input_file, IDENT);
				readNext(input_file, OBNAME);
				readNext(input_file, IDENT);
				break;
			} // switch (code)
		} // if (count...)
	} // if (elsize...)
} // Value::readNext()

template <class Out>
void Value::writeNext(Out &out, Code code, size_t count) {
	using namespace Representation;
	if (count == 0) return;
	const byte *p = m_praw;
	// - см. комментарий к (аналогичному по смыслу) определению pos в readNext
	uint elsize = getSize(code);
	if (elsize != -1) {
		size_t size = elsize * count;
		writeBytesNext(out, size);
	}
	else {	// Значение переменной длины
		if (count > 1)
			for (uint32 n = 0; n < count; ++n) writeNext(out, code);
		else {
			switch (code) {
			case UVARI: case ORIGIN: {
				writeBytesNext(out, 1);		// Первый байт кода UVARI
				uint len = Convertor::getSizeUVARI(*p);
				if (len > 1) writeBytesNext(out, len - 1);
				break;
			}
			case ASCII:
			case IDENT:	case UNITS: {
				uint32 strLen;
				if (code == ASCII) {
					writeNext(out, UVARI);
					Convertor::fromRaw(strLen, p, UVARI);
					// - NB: Не проверяем код возврата, т.к. используется uint32
				}
				else {
					writeNext(out, USHORT);
					strLen = *p;
				}
				writeBytesNext(out, strLen);
				break;
			}
			case OBNAME:
				writeNext(out, ORIGIN);
				writeNext(out, USHORT);
				writeNext(out, IDENT);
				break;
			case OBJREF:
				writeNext(out, IDENT);
				writeNext(out, OBNAME);
				break;
			case ATTREF:
				writeNext(out, IDENT);
				writeNext(out, OBNAME);
				writeNext(out, IDENT);
				break;
			} // switch (code)
		} // if (count...)
	} // if (elsize...)
} // Value::writeNext()

template <class Out>
/*static*/ void Value::writeNext(Out &out, const byte *&p,
								 Code code, size_t count) {
	using namespace Representation;
	if (count == 0) return;
	const byte *pFrom = p;
	// - см. комментарий к (аналогичному по смыслу) определению pFromos в readNext
	uint elsize = getSize(code);
	if (elsize != -1) {
		long size = elsize * count;
		writeBytesNext(out, p, size);
	}
	else {	// Значение переменной длины
		if (count > 1)
			for (uint32 n = 0; n < count; ++n) writeNext(out, p, code);
		else {
			switch (code) {
			case UVARI: case ORIGIN: {
				writeBytesNext(out, p, 1);		// Первый байт кода UVARI
				uint len = Convertor::getSizeUVARI(*pFrom);
				if (len > 1) writeBytesNext(out, p, len - 1);
				break;
			}
			case ASCII:
			case IDENT:	case UNITS: {
				uint32 strLen;
				if (code == ASCII) {
					writeNext(out, p, UVARI);
					Convertor::fromRaw(strLen, pFrom, UVARI);
					// - NB: Не проверяем код возврата, т.к. используется uint32
				}
				else {
					writeNext(out, p, USHORT);
					strLen = *pFrom;
				}
				writeBytesNext(out, p, strLen);
				break;
			}
			case OBNAME:
				writeNext(out, p, ORIGIN);
				writeNext(out, p, USHORT);
				writeNext(out, p, IDENT);
				break;
			case OBJREF:
				writeNext(out, p, IDENT);
				writeNext(out, p, OBNAME);
				break;
			case ATTREF:
				writeNext(out, p, IDENT);
				writeNext(out, p, OBNAME);
				writeNext(out, p, IDENT);
				break;
			} // switch (code)
		} // if (count...)
	} // if (elsize...)
} // Value::writeNext()

//template <typename T>
///*static*/ RI Value::getNext(T &var, Code code, long index) {
////	Взять код из SingleValue::get
//}

//template <class T>
///*static*/ RI Value::getNext(vector<T> &vec, Code code, size_t count) {
////	Взять код из SingleValue::getAll
//}

// end of class Value definitions

class SingleValue : public Value {
public:
	template <typename T>
		static RI read(T &var, Input &input_file, Code code);
	template <typename T>
		static RI write(Output &out, const T &var, Code code);
	SingleValue() { clear(); }
	template <typename T>
		RI get(T *p, size_t index, size_t count) const;
	template <typename T>
		RI set(const T *p, size_t count, Code code);
	void clear() {
		m_raw.clear();
		m_rc = Representation::Undefined;
		m_cnt = 0;
	}
	void setNull(Code code, size_t count);
	template <typename In>
	void read(In &input_file, Code code, size_t count = 1) {
		m_raw.clear();
		readNext(input_file, code, count);
		m_rc = code;
		m_cnt = count;
	}
	template <class Out>
	void write(Out &out) const {
		if (empty()) return;
		const byte *p = cfirstBytePtr();
		writeNext(out, p, m_rc, m_cnt);
	}
private:
	struct AnyType {	// - поддержка значений любого типа
		template <typename T>
			static bool cannotHoldCode(const T &val, Code c) { return false; }
		template <typename T>
			static bool cannotHoldType(Code c, const T &val) { return false; }
		template <typename T>
			static bool cannotHoldValue(Code c, const T &val) { return false; }
		// - NB: Возвращаемое значение false функций cannotHoldXXX класса
		//		 AnyType имеет смысл "неизвестно"
		static bool needsPriorCheck() { return false; }
		/* - возвращает признак необходимости предварительного вызова функции
			 cannotHoldValue для проверки всех элементов значения перед
			 циклом их записи (в функции SingleValue::set) */
		template <typename T>
		static RI fromRaw(T &val, const byte *&p, Code c, bool testInt) {
			return Convertor::fromRaw(val, p, c);
		}
	};
	struct Strings : public AnyType {
		static bool cannotHoldType(Code c, const string &val) {
			return c == Representation::IDENT || c == Representation::UNITS;
		}
		static bool cannotHoldValue(Code c, const string &val) {
			using namespace Representation;
			switch (c) {
			case IDENT:
				return !Ident::valid(val);
			case UNITS:
				return !Units::valid(val);
			case ASCII:
				return val.length() > Representation::maxUVARI ||
					   val.find_first_not_of(CharSets::ascii) != string::npos;
			default:
				return false;
			}
		}
		// - Проверяет соответствие стандарту DLIS
		static bool needsPriorCheck() { return true; }
	};
	struct Singles : public AnyType {
		static bool cannotHoldCode(const ieeeSingle &val, Code c) {
			return c == Representation::FDOUBL;
		}
	};
	struct IntType : public AnyType {	// - поддержка значений целого типа
		template <typename T>
		static bool cannotHoldCode(const T &val, Code c) {
			return !DlisIntTypes::canHoldCode(val, c);
		}
		template <typename T>
		static bool cannotHoldType(Code c, const T &val) {
			return !DlisIntTypes::canHoldType(c, val);
		}
		template <typename T>
		static bool cannotHoldValue(Code c, const T &val) {
			return !DlisIntTypes::canHoldValue(c, val);
		}
		template <typename T>
		static RI fromRaw(T &val, const byte *&p, Code c, bool testInt) {
			return Convertor::fromRawIntDisp(val, p, c, testInt);
		}
	};
	template <typename T> struct Type : AnyType {};
	template<> struct Type<byte>   : IntType {};
	template<> struct Type<int8>   : IntType {};
	template<> struct Type<uint16> : IntType {};
	template<> struct Type<int16>  : IntType {};
	template<> struct Type<uint32> : IntType {};
	template<> struct Type<int32>  : IntType {};
	template<> struct Type<string>  : Strings {};
	template<> struct Type<ieeeSingle>  : Singles {};
// - NB: Лучше перенести эти функции в класс DlisIntTypes
// ----------------------------------------------------------------------------
	Code m_rc;
	uint32 m_cnt;
};

template <typename T>
/*static*/ RI SingleValue::read(T &var, Input &input_file, Code code) {
	SingleValue v;
	v.read(input_file, code);
	return v.get(&var, 0, 1);
}

template <typename T>
/*static*/ RI SingleValue::write(Output &out, const T &var, Code code) {
	return Convertor::toRaw(out, var, code);
}

void SingleValue::setNull(Code code, size_t count) {
	using namespace Representation;
	m_raw.clear();
	if (code == Undefined || count == 0) {
		m_rc = Undefined;
		m_cnt = 0;
		return;
	}
	m_rc = code;
	m_cnt = count;
	uint size = getSize(code);
	if (size == -1)
		switch (code) {
		case OBNAME: size = 3; break;
		case OBJREF: size = 4; break;
		case ATTREF: size = 5; break;
		default: size = 1;
		}
	m_raw.assign(count * size, 0);
}

//template <typename T>
//RI SingleValue::get(T *pTo, size_t index, size_t count) const {
//		assert(index + count <= m_cnt);
//	if (m_cnt == 0) return RI(RI::NoVal).toCritical();
//	if (index >= m_cnt) return RI(RI::BigValIndex).toCritical();
//	const byte *p = cfirstBytePtr();
//// Пропускаем элементы с индексами, меньшими чем index
//	uint sz = Representation::getSize(m_rc);
//	if (sz != -1)	// - если элементы фиксированного размера
//		p += index * sz;
//	else
//		for (size_t n = 0; n < index; ++n) skip(p, m_rc);
//// Считываем m_cnt элементов и возвращаем "накопленный" код возврата

////	RI ri;
////	for (size_t n = 0; n < count; ++n)
////		ri.upTo(Convertor::fromRaw(*pTo++, p, m_rc));
////	return ri;

//	if (count == 1)
//		return Convertor::fromRaw(*pTo, p, m_rc);
//	else {
//		RI riAll, ri;
//		for (size_t n = 0; n < count; ++n) {
//			ri = Convertor::fromRaw(*pTo++, p, m_rc);
//			if (ri.critical()) return ri;
//			riAll.upTo(ri);
//		}
//		return riAll;
//	}
//}

template <typename T>
RI SingleValue::get(T *pTo, size_t index, size_t count) const {
		assert(index + count <= m_cnt);
	if (m_cnt == 0) return RI(RI::NoVal).toCritical();
	if (index >= m_cnt) return RI(RI::BigValIndex).toCritical();
	const byte *p = cfirstBytePtr();
	if (index > 0) {
	// Пропускаем элементы с индексами, меньшими чем index
		uint sz = Representation::getSize(m_rc);
		if (sz != -1)	// - если элементы фиксированного размера
			p += index * sz;
		else
			for (size_t n = 0; n < index; ++n) skip(p, m_rc);
	}
	if (count == 1)
		return Type<T>::fromRaw(*pTo, p, m_rc, true);
	else {
		if (Type<T>::cannotHoldCode(*pTo, m_rc))
			return RI(RI::FromRawTypeErr, 4).toCritical();
	// Считываем m_cnt элементов и возвращаем "накопленный" код возврата
		RI riAll, ri;
		for (size_t n = 0; n < count; ++n) {
			ri = Type<T>::fromRaw(*pTo++, p, m_rc, false);
			if (ri.critical()) return ri;
/* - NB: Условие ri.critical()==true должно выполняться только на первом
		 элементе значения (n==0), и оно подразумевает невозможность
		 преобразования любого значения в коде m_rc в тип T (однако логичнее
		 было бы такую проверку полностью вынести в функции cannotHoldXXX
		 классов - наследников AnyType, чтобы в этой проверке не было бы
		 необходимости)
		 Исключение: Ошибка формата входного значения в Convertor::fromRaw() */
			riAll.upTo(ri);
		}
		return riAll;
	}
}

//template <typename T>
//RI SingleValue::set(const T *p, size_t count, Code code) {
//	clear();

////	m_rc = code;
////	m_cnt = count;
////	RI ri;
////	if (m_cnt == 1)
////		ri = Convertor::toRaw(*this, *p, m_rc);
////	else
////		for (size_t n = 0; n < m_cnt; ++n)
////			ri.upTo(Convertor::toRaw(*this, *p++, m_rc));
////	return ri;

//	RI riAll, ri;
//	if (count == 1) {
//		riAll = Convertor::toRaw(*this, *p, code);
//		if (riAll.critical()) return riAll;
//	}
//	else
//		for (size_t n = 0; n < count; ++n) {
//			ri = Convertor::toRaw(*this, *p++, code);
//			if (ri.critical()) return ri;
//			riAll.upTo(ri);
//		}
//	m_rc = code;
//	m_cnt = count;
//	return riAll;
//}

//template <typename T>
//RI SingleValue::set(const T *p, size_t count, Code code) {
//	clear();
//	RI riAll, ri;
//	if (count == 1) {
//		if (Type<T>::cannotHoldValue(code, *p))
//			return RI(RI::ToRawValErr, 2).toCritical();
//		riAll = Convertor::toRaw(*this, *p, code);
//		if (riAll.critical()) return riAll;
//	} else {
//		if (Type<T>::cannotHoldType(code, *p))
//			return RI(RI::ToRawTypeErr, 3).toCritical();
//		for (size_t n = 0; n < count; ++n) {
//			ri = Convertor::toRaw(*this, *p++, code);
//			if (ri.critical()) return ri;
//			riAll.upTo(ri);
//		}
//	}
//	m_rc = code;
//	m_cnt = count;
//	return riAll;
//}

template <typename T>
RI SingleValue::set(const T *p, size_t count, Code code) {
	clear();
	RI riAll, ri;
	if (count == 1) {
		if (Type<T>::cannotHoldValue(code, *p))
			return RI(RI::ToRawValErr, 2).toCritical();
		riAll = Convertor::toRaw(*this, *p, code);
		if (riAll.critical()) return riAll;
	} else {
		if (Type<T>::cannotHoldType(code, *p))
			return RI(RI::ToRawTypeErr, 3).toCritical();
		if (Type<T>::needsPriorCheck()) {
			const T *pBak = p;
			for (size_t n = 0; n < count; ++n) {
				if (Type<T>::cannotHoldValue(code, *p++))
					return RI(RI::ToRawValErr, 3).toCritical();
			}
			p = pBak;
		}
		for (size_t n = 0; n < count; ++n) {
			ri = Convertor::toRaw(*this, *p++, code);
			if (ri.critical()) return ri;
			riAll.upTo(ri);
		}
	}
	m_rc = code;
	m_cnt = count;
	return riAll;
}


// ============================================================================

class VisibleRecord {
public:
	static const uint limitLength = 16384;
//	static const uint limitDataLength = limitLength - Header::getSize();
// - NB: Ошибка компиляции
//	const uint limitDataLength = limitLength - Header::getSize();
// - NB: Нет ошибки компиляции, но тогда limitDataLength будет присутствовать
//		 в каждом экземпляре VisibleRecord
	static const uint limitDataLength()
		{ return limitLength - Header::getSize(); }
	class Header {
	public:
		static const uint getSize() { return sizeof Header; }
//		Header() : m_vrl(0) { reset(); }
//		void reset() {
//			m_fmt[0] == 0xff;
//			m_fmt[1] == 0x01;
//		}
		Header() : m_vrl(0) {}	// - для чтения
		// Конструктор для записи:
		Header(uint16 recordDataLength) : m_vrl(recordDataLength + getSize()) {
				if (m_vrl > limitLength) throw RI(RI::ProgErr, 5);
			m_fmt[0] = 0xff;
			m_fmt[1] = 0x01;
		}						// - для записи
		size_t getRecordDataLength() const { return m_vrl - getSize(); }
		void write(Output &out) const {
			if (littleEndian) reverseBytes(m_vrl);
			out.write(this, getSize());
			if (littleEndian) reverseBytes(m_vrl);
		}

		void read(Input &input_file) 
        {
			input_file.read(this, getSize());

			if (littleEndian) 
                reverseBytes(m_vrl);

			if (!isValid())
				throw RI(RI::BadVRH);
		}

	private:
		bool isValid() const {
			return (m_vrl <= limitLength) &&
				   (m_fmt[0] == 0xff) && (m_fmt[1] == 0x01);
			// - NB: Для упрощения сравниваем m_rl с общим ограничением, а не
			//		 с актуальным значением StorageUnitLabel.getMaxVisRecLen()
		}
//		size_t getSize() const { return sizeof(*this); }
// - NB: Проверить на alignment!
		uint16 m_vrl;	// Visible Record Length
		byte m_fmt[2];	// Format Version
	}; // class Header --------------------------------------------------------
// Поддержка LogicalFile::Impl::readBuffered
	VisibleRecord() : m_pData(NULL), m_dl(0) {}
	~VisibleRecord() { delete m_pData; }
	uint16 dataLength() const { return m_dl; }
	uint16 length() const { return Header::getSize() + m_dl; }

// Функции чтения
	std::istream *pData() 
    { 
        return m_pData; 
    }

	// - Данные видимой записи как входной поток
	void read(Input &file_DLIS) 
    {
		Header  vrh;
        
		vrh.read(file_DLIS);
        
		delete m_pData;
		m_pData = new std::stringstream(modeStreamBinInOut);

		file_DLIS.read(*m_pData, vrh.getRecordDataLength());
		m_dl    =  (*m_pData).tellp();

	}

// Функции записи
	std::ostream *pOutData() 
    { 
        return m_pData; 
    }

	// - Данные видимой записи как выходной поток
	size_t outDataLength() 
    { 
        return (*m_pData).tellp(); 
    }

	bool outEmpty() 
    { 
        return outDataLength() == 0; 
    }

	long outSpaceLeft() 
    { 
        return limitDataLength() - outDataLength(); 
    }

// - NB: При отсутствии ошибок функция outSpaceLeft должна возвращать
//		 неотрицательное значение
	void clear() 
    {
		delete m_pData;
		m_pData = NULL;
	}
	void newOutRecord() {
		delete m_pData;
		m_pData = new std::stringstream(modeStreamBinInOut);
	}
	void write(Output &outDlis) {
		size_t dl = outDataLength();
		Header vrh(dl);
		vrh.write(outDlis);
		outDlis.write(*m_pData, dl);
	}
private:
	std::stringstream *m_pData;
	uint16 m_dl; // - NB: используется только при чтении
}; // class VisibleRecord

// ============================================================================

// Положение логической записи во входном DLIS-потоке
class LogicalRecordLocation {
public:
//	LogicalRecordLocation(std::streampos visRecPos = 0, std::streamoff ofs = 0)
//		: visRecPos(visRecPos), ofs(ofs) {}
	std::streampos visRecPos;
	// - Позиция видимой записи, содержащей первый сегмент
	std::streamoff ofs;
	// - Позиция первого сегмента в этой видимой записи
	void clear() {
		visRecPos = 0;
		ofs = 0;
	}
};

enum EFLRType 		// Типы Explicitly Formatted Logical Record
{
	FHLR =	 0,	    // File Header
	OLR =	 1,	    // Origin
	AXIS =	 2,	    // Coordinate Axis
	CHANNL = 3,	    // Channel-related information
	FRAME =  4,	    // Frame Data
	STATIC = 5,	    // Static Data
	SCRIPT = 6,	    // Textual Data
//	UPDATE = 7	    // Update Data
	/*...*/
};

class LogicalRecord {
public:
	enum IFLRType   // Типы of Indirectly Formatted Logical Record
    {
		FDATA =	 0,	// Frame Data
		/*...*/
	};

	struct Type 
    {
		Type() : isEFLR(false), i(IFLRType(0)) {}	// - для чтения
		Type(EFLRType e) : isEFLR(true), e(e) {}	// - для записи
		Type(IFLRType i) : isEFLR(false), i(i) {}	// - для записи
		bool isEFLR;
		union 
        {
			EFLRType e;
			IFLRType i;
		};
	};
	LogicalRecord() : m_pBody(NULL) { newRecord(); }	// - для чтения
	LogicalRecord(EFLRType e) : m_lrt(e), m_pBody(NULL), m_new(false)
		{ newOutRecord(); }								// - для записи
	LogicalRecord(IFLRType i) : m_lrt(i), m_pBody(NULL), m_new(false)
		{ newOutRecord(); }								// - для записи
	~LogicalRecord() { delete m_pBody; }
	Type getType() const { return m_lrt; }
	bool isEFLR() const { return m_lrt.isEFLR; }
	bool isFileHeader() const { return isEFLR() && m_lrt.e == FHLR; }
// - NB: Функции isXXX доступны уже после чтения первого сегмента
	bool encrypted() const { return m_lrsh1.lrsa[Encrypt]; }
//	std::istream *pBody() { return m_pBody; }
	std::istream &body() { return *m_pBody; }
	std::istream *extractBody() {
		std::istream *pb = m_pBody;
		m_pBody = NULL;
		return pb;
	}
	// - Возвращает извлеченное тело логической записи как входной поток
// Функции чтения
	void newRecord(bool toReadBody = true) 
    {
		delete m_pBody;
		m_pBody = NULL;
		if (toReadBody)
			m_pBody = new std::stringstream(modeStreamBinInOut);
		m_new = true;
	}
	/* - Используется перед чтением первого сегмента каждой последующей
		 логической записи; о параметре toReadBody - см. ниже комментарий
		 к readSegment() */
	void readSegment(Input &input_file, bool &wasLast);
	/* - Считывает из in очередной сегмент логической записи, извлекает его
		 тело (Logical Record Body) и записывает его в собственный поток
		 (см. функцию pBody); если функция newRecord() перед этим была вызвана
		 с параметром false, считывается только заголовок сегмента,
		 тело пропускается и поток устанавливается на конец сегмента;
		 выходной параметр wasLast содержит признак того, что был прочитан
		 последний сегмент записи */
// Функции записи
	void newOutRecord() {
		delete m_pBody;
		m_pBody = new std::stringstream(modeStreamBinInOut);
	}
	/* - Используется перед записью первого сегмента каждой последующей
		 логической записи */
	std::ostream *pOutBody() { return m_pBody; }
	uint writeSegments(Output &out, uint spaceLeft, bool &wereLast);
	/* - Формирует и записывает в out очередную порцию сегментов логической
		 записи, общим размером не более spaceLeft, и возвращает их количество;
		 выходной параметр wereLast содержит признак того, что были записаны
		 последние сегменты записи */
		// Поле, заполняемое и используемое только из LogicalFile::Impl
		LogicalRecordLocation location;

private:
	enum SegmentAttributeBitPos 
    {
		Padding    = 0,
		TrailLen   = 1,
		Checksum   = 2,
		EncryptPkt = 3,
		Encrypt    = 4,
		NotLast    = 5,
		NotFirst   = 6,
		EFLR	   = 7
	};

	// - Значения элементов перечисления SegmentAttributeBitPos задают
	//	 нoмера бит в SegmentHeader.hdr.lrsab
	static const uint lowlimitSegmentLength = 16;
	struct SegmentHeader 
    {
		struct HeaderData 
        {
			HeaderData() : lrsl(0), lrsab(0), lrtb(0) {}

			uint16 lrsl;	        // Segment Length
			byte   lrsab;		    // Segment Attributes byte
			byte   lrtb;		    // Logical Record Type byte
		};

		static const uint getSize() 
        { 
            return 4; 
        }

		SegmentHeader() {}					    // - для чтения
//		SegmentHeader(uint16 segDataLen) {		// - для записи
//			hdr.lrsl = segDataLen + getSize();
//		}
// - исправлено:
		SegmentHeader(uint16 segLen)    // - для записи
        { 
            hdr.lrsl = segLen; 
        }	

		int getSegmentDataLength() const 
        { 
            return hdr.lrsl - getSize(); 
        }

		void read(Input &input_file) 
        {
			if (sizeof hdr != getSize()) 
                throw RI(RI::Platform);

			input_file.read(hdr);

			if (littleEndian) 
                reverseBytes(hdr.lrsl);

			if (getSegmentDataLength() < 0)
				throw RI(RI::BadLRSH, 1);

			if ((hdr.lrsl % 2 != 0) || (hdr.lrsl < lowlimitSegmentLength))
				input_file.addIssue(RI::NonStdLRSH);

			lrsa = hdr.lrsab;
		}

		void write(Output &out)
        {
				if (sizeof hdr != getSize()) throw RI(RI::Platform);
			hdr.lrsab = lrsa.to_ulong();
			if (littleEndian) reverseBytes(hdr.lrsl);
			out.write(hdr);
			if (littleEndian) reverseBytes(hdr.lrsl);
		}

		HeaderData hdr;
		std::bitset<8> lrsa;	// Segment Attributes
	}; // struct SegmentHeader ------------------------------------------------
//	void writeSegment(Output &out, uint segBodyLen, bool isFirst, bool isLast);
	uint writeSegment(Output &out, uint segBodyLen, bool isFirst, bool isLast);
	// - Записывает в out очередной сегмент с длиной тела segBodyLen
	//	 и возвращает полную длину записанного сегмента
// Data -----------------------------------------------------------------------
	Type m_lrt;
	std::stringstream *m_pBody;
// Только для чтения
	SegmentHeader m_lrsh1;	// Заголовок первого сегмента
	bool m_new;
//	RI riLRB;
}; // class LogicalRecord

void LogicalRecord::readSegment(Input &input_file, bool &was_last) 
{
	struct SegmentSize 
    {
		SegmentSize(int initSize) : body(initSize), trailer(0) {}

		void decreaseBody(int d) 
        {
			body    -= d;
			trailer += d;
		}

		int     body;
        int     trailer;
	};

	SegmentHeader lrsh2;	     // Заголовок не первого сегмента
	SegmentHeader &lrsh = m_new ? m_lrsh1 : lrsh2;

	lrsh.read(input_file);
    
    if (m_new)
        if ( lrsh.lrsa[NotFirst])
            throw RI(RI::BadLRSH, 2);

	if (m_new)
    {
		m_lrt.isEFLR = m_lrsh1.lrsa[EFLR];

		if (isEFLR()) 
            m_lrt.e = EFLRType(lrsh.hdr.lrtb);
		else 
            m_lrt.i = IFLRType(lrsh.hdr.lrtb);

		/* 
        NB: Использовать присваивание вида
		reinterpret_cast<byte &>(m_lrt.e) = lrsh.m_lrt;
		нельзя, т.к. размер m_lrt.e не обязательно 1 байт 
        */
	}
	else 
    {
		bool ok = (lrsh.lrsa[EFLR] == m_lrsh1.lrsa[EFLR]) && (lrsh.hdr.lrtb == m_lrsh1.hdr.lrtb);
		if (!ok) 
            throw RI(RI::BadLRSH, 3);
		// - NB: Проверка правильности установки битов Encrypt и EncryptPkt
		//		 не выполняется
	}

	SegmentSize ss(lrsh.getSegmentDataLength());

	if (!m_pBody || lrsh.lrsa[Encrypt])    // Пропускаем сегмент
		input_file.offset(ss.body);	
	else 
    {
		if (lrsh.lrsa[TrailLen]) 
            ss.decreaseBody(2);

		if (lrsh.lrsa[Checksum]) 
            ss.decreaseBody(2);

		if (lrsh.lrsa[Padding]) 
        {
			std::streampos p = input_file.getPos();
			input_file.offset(ss.body - 1);

			byte    padCnt;
			input_file.read(padCnt);
			if (padCnt == 0) 
                throw RI(RI::BadLRSH, 4);

			// - т.к. счетчик в байте padCnt включает в себя сам этот байт
			ss.decreaseBody(padCnt);
			if (ss.body < 0) 
                throw RI(RI::BadLRSH, 5);

			input_file.setPos(p);
		}
		input_file.read(*m_pBody, ss.body);

		// - Добавляем в *m_pBody тело очередного сегмента
		if (ss.trailer > 0) 
            input_file.offset(ss.trailer);
	}

	m_new    = false;
	was_last = !lrsh.lrsa[NotLast];

} // LogicalRecord::readSegment

//void LogicalRecord::writeSegment(Output &out, uint segBodyLen,
//								 bool isFirst, bool isLast) {
//	bool padToEven = segBodyLen % 2 == 1;
//	SegmentHeader lrsh(segBodyLen + (padToEven ? 1 : 0));
//	lrsh.lrsa[Padding] = padToEven;
//	lrsh.lrsa[NotLast] = !isLast;
//	lrsh.lrsa[NotFirst] = !isFirst;
//	lrsh.lrsa[EFLR] = isEFLR();
//	lrsh.hdr.lrtb = isEFLR() ?  m_lrt.e : m_lrt.i;
//	lrsh.write(out);
//	out.write(*m_pBody, segBodyLen);
//	if (padToEven) out.write((byte)1);
//}
// - исправлено:

uint LogicalRecord::writeSegment(Output &out, uint segBodyLen, bool isFirst, bool isLast) 
{
	uint segLen  = segBodyLen + SegmentHeader::getSize();
	byte padding = segLen % 2;	// Дополнение (сначала до четной длины)

	segLen += padding;
	if (segLen < lowlimitSegmentLength)
    {
		uint paddingToMinSegLen = lowlimitSegmentLength - segLen;
		// - дополнение до минимальной длины сегмента
		padding += paddingToMinSegLen;
		segLen += paddingToMinSegLen;
	}
	SegmentHeader lrsh(segLen);
	lrsh.lrsa[Padding] = padding > 0;
	lrsh.lrsa[NotLast] = !isLast;
	lrsh.lrsa[NotFirst] = !isFirst;
	lrsh.lrsa[EFLR] = isEFLR();
	lrsh.hdr.lrtb = isEFLR() ?  m_lrt.e : m_lrt.i;
	lrsh.write(out);
	out.write(*m_pBody, segBodyLen);
	if (padding > 0) {
		if (padding > 1) {
			vector<byte> padBytes(padding - 1);
			out.write(&padBytes.front(), padBytes.size());
		}
		out.write(padding);
	}
	return segLen;
}

//uint LogicalRecord::writeSegments(Output &out, uint spaceLeft,
//								  bool &wereLast) {
//	long bytesLeft = m_pBody->tellp() - m_pBody->tellg();
//		if (bytesLeft == 0) {
//			wereLast = true;
//			return 0;
//		}
//		// - на случай избыточного повторного вызова
//	wereLast = false;
//	uint segCnt = 0;
//	do {
//			if (bytesLeft < 0) throw RI(RI::ProgErr, 6);

//		int maxSegBodyLen = (int)spaceLeft - SegmentHeader::getSize();
//		// - NB: предполагая, что spaceLeft и SegmentHeader::getSize() четные
//		if (maxSegBodyLen < 0) break;
////			if (maxSegBodyLen < (int)lowlimitSegmentLength - SegmentHeader::getSize()) break;
//		uint segBodyLen = std::min(bytesLeft, (long)maxSegBodyLen);

////		if (spaceLeft < lowlimitSegmentLength) break;
////		uint segBodyLen = std::min(bytesLeft, (long)spaceLeft - SegmentHeader::getSize());

//		bool isFirst = m_pBody->tellg() == (std::streampos)0;
//		bool isLast = bytesLeft == segBodyLen;
//		writeSegment(out, segBodyLen, isFirst, isLast);
//		wereLast = isLast;
//		bytesLeft -= segBodyLen;
//		spaceLeft -= segBodyLen + segBodyLen % 2 + SegmentHeader::getSize();
//		++segCnt;
//	} while (spaceLeft >= lowlimitSegmentLength && !wereLast);
//	return segCnt;
//}
// - исправлено:
uint LogicalRecord::writeSegments(Output &out, uint spaceLeft,
								  bool &wereLast) {
	long bytesLeft = m_pBody->tellp() - m_pBody->tellg();
		if (bytesLeft == 0) {
			wereLast = true;
			return 0;
		}
		// - на случай избыточного повторного вызова
	wereLast = false;
	uint segCnt = 0;
	while (spaceLeft >= lowlimitSegmentLength && !wereLast) {
			if (bytesLeft < 0) throw RI(RI::ProgErr, 6);
		int maxSegBodyLen = (int)spaceLeft - SegmentHeader::getSize();
		// - NB: предполагая, что spaceLeft и SegmentHeader::getSize() четные
		if (maxSegBodyLen < 0) break;
		uint segBodyLen = std::min(bytesLeft, (long)maxSegBodyLen);
		bool isFirst = m_pBody->tellg() == (std::streampos)0;
		bool isLast = bytesLeft == segBodyLen;
		uint segLen = writeSegment(out, segBodyLen, isFirst, isLast);
		wereLast = isLast;
		bytesLeft -= segBodyLen;
		spaceLeft -= segLen;
		++segCnt;
	}
	return segCnt;
}

// ============================================================================

class AttributeDefinition {
public:
	typedef Representation::Code RCode;
	static const RCode rNone = Representation::Undefined;
	AttributeDefinition(bool tagged = false, uint cnt = -1,
						RCode rc = rNone, RCode rc2 = rNone) :
		tagged(tagged), restricted(cnt)
	{
		restricted.valueCount = cnt;
		if (rc != rNone) restricted.representationCodes.push_back(rc);
		if (rc2 != rNone) restricted.representationCodes.push_back(rc2);
	}
	AttributeDefinition(bool tagged, RCode rc, RCode rc2 = rNone) :
		tagged(tagged) {
		restricted.representationCodes.push_back(rc);
		if (rc2 != rNone)
			restricted.representationCodes.push_back(rc2);
	}
/* - NB: Параметр rc2 в перегруженном конструкторе
		 AttributeDefinition(bool , RCode, RCode) нужен хотя бы потому, что
		 без него при вызове AttributeDefinition(bool, RCode, RCode) выполнится
		 первый конструктор, что неправильно */
	bool validCount(uint32 cnt) const {
		uint vcnt = restricted.valueCount;
		return vcnt == -1 || cnt == vcnt;
	}
	bool validReprCode(RCode code) const {
		const vector<RCode> &codes = restricted.representationCodes;
		if (codes.size() == 0) return true;
		for (uint n = 0; n < codes.size(); ++n)
			if (code == codes[n]) return true;
		return false;
	}
// Data
	bool tagged;
	/* - Признак автоматически добавляемых атрибутов; в классах - наследниках
		 Object к ним можно обращаться также через выделенные функции
		 (с помощью тегов) */
	struct Restrictions {
		Restrictions(uint cnt = -1) : valueCount(cnt) {}
		uint valueCount;	// -1 означает отсутствие ограничений
		vector<RCode> representationCodes;
	} restricted;
};

// Таблица определений атрибутов
class AttributeMap {
public:
	typedef Representation::Code RCode;
	typedef std::pair<const string, const AttributeDefinition> DefinitionPair;
	AttributeMap() : tagged(false) {}
	uint count() const { return mvr_defs.size(); }
	const AttributeDefinition *definition(const string &attrLabel) const {
		Map::const_iterator it = mm_defs.find(attrLabel);
		return it != mm_defs.end() ? &it->second : NULL;
	}
	const DefinitionPair &definitionPair(uint index) const
		{ return *mvr_defs.at(index); }
// - NB: Здесь выполняется разыменование итератора, а не указателя
	void add(const string &attrLabel, const AttributeDefinition &ad) {
		std::pair<Map::iterator, bool> insertInfo =
				mm_defs.insert(std::make_pair(attrLabel, ad));
		if (insertInfo.second) mvr_defs.push_back(insertInfo.first);
	}
//	AttributeDefinition *add(const string &attrLabel,
//							 const AttributeDefinition &ad) {
//		std::pair<Map::iterator, bool> insertInfo =
//				mm_defs.insert(std::make_pair(attrLabel, ad));
//		if (insertInfo.second) mvr_defs.push_back(insertInfo.first);
//		return insertInfo.first->second;
//	}
//	AttributeDefinition *add(const string &attrLabel, uint cnt,
//							 RCode rc = Representation::Undefined,
//							 RCode rc2 = Representation::Undefined) {
//		return add(attrLabel, AttributeDefinition(cnt, rc, rc2).toTagged(tagged));
//	}
//	AttributeDefinition *add(const string &attrLabel, RCode rc) {
//		return add(attrLabel, AttributeDefinition(rc).toTagged(tagged));
//	}
	void add(const string &attrLabel, uint cnt = -1,
			 RCode rc = Representation::Undefined,
			 RCode rc2 = Representation::Undefined) {
		add(attrLabel, AttributeDefinition(tagged, cnt, rc, rc2));
	}
	void add(const string &attrLabel, RCode rc) {
		add(attrLabel, AttributeDefinition(tagged, rc));
	}
	bool tagged;
	// - Признак AttributeDefinition::tagged для следующего добавляемого
	//	 определения
private:
	typedef std::map<const string, const AttributeDefinition> Map;
	Map mm_defs;
	// - Ассоциативная таблица определений, сортированная по Attribute::label
	vector<Map::iterator> mvr_defs;
	// - Ссылки на эти определения в исходном порядке их добавления
	// - NB: этот список нужен только в "эстетических" целях: для того, чтобы
	//		 обязательные атрибуты добавлялись в указанном порядке
};

// ============================================================================

class Component {		// Базовый класс для всех типов компонент
public:
	enum Role {					// Component Role
		rAbsentAttr = 0,	// Absent Attribute
		rAttr		= 1,	// Attribute
		rInvarAttr  = 2,	// Invariant Attribute
		rObject		= 3,
		rReserved	= 4,
		rRdSet		= 5,	// Redundant Set
		rRepSet		= 6,	// Replacement Set
		rSet		= 7
	};
	enum RoleGroup { rgAttr, rgObject, rgReserved, rgSet };
	typedef std::bitset<5> FormatBits;

	class Descriptor 			// Component Descriptor
    {
	public:
		Descriptor() : role(Role(0)) {}

		void read(Input &input_file) 
        {
			byte    m_db;               // Component Descriptor byte

			input_file.read(m_db);

			role = Role(m_db >> 5);     // 3 старших бита из байта m_db
			fmt  = m_db;                // 5 младших битов из байта m_db
		}

		void write(Output &out) const 
        {
			byte m_db = (role << 5) + fmt.to_ulong();

			out.write(m_db);
		}
	// Data -------------------------------------------------------------------
		Role role;
		FormatBits fmt;
	}; // class Descriptor ----------------------------------------------------
	static RoleGroup roleGroupOf(Role r) {
		switch (r) {
		case rAbsentAttr: case rAttr: case rInvarAttr:	return rgAttr;
		case rObject:									return rgObject;
		case rReserved:									return rgReserved;
		case rRdSet: case rRepSet: case rSet:			return rgSet;
			default: assert(false);
		}
	}
		virtual RoleGroup roleGroup() = 0;
	virtual void read(Input &input_file) 
    {
		reset();
		m_d.read(input_file);

        if (roleGroupOf(m_d.role) != roleGroup()) 
            throw RI(RI::ProgErr, 7);

		readCharacteristics(input_file);
	}

	virtual void write(Output &out) const 
    {
		m_d.write(out);
		writeCharacteristics(out);
	}
	Role role() const { return m_d.role; }
	FormatBits format() const { return m_d.fmt; }
protected:
	virtual void reset() = 0;
// - NB: В C++ виртуальные функции при вызове из конструктора не являются
//		 виртуальными, поэтому инициализирующую функцию reset() нужно вызывать
//		 из конструкторов каждого производного класса
	virtual void readCharacteristics(Input &input_file) = 0;
	virtual void writeCharacteristics(Output &out) const = 0;
// Data -------------------------------------------------------------------
	Descriptor m_d;
}; // class Component

// ============================================================================

class ValueInterface::Dispatcher {
public:
	Dispatcher(Attribute *a) : m_isChanRef(false), m_ra(a) {}
	Dispatcher(Channel *c) : m_isChanRef(true), m_rc(c) {}
	size_t getCount() const;
//	template <class T>
//		RI getValue(T &value, size_t index = -1) const;
//	template <class T>
//		RI getValue(vector<T> &value) const;
//// - NB: Функции getValue можно попробовать объединить, по аналогии с setValue
////		 (т.к. они являются шаблонами, это было бы полезно и с точки зрения
////		 уменьшения размера кода)
	template <class T>
		RI getValue(T *p, size_t index, size_t count) const;
	template <class T>
		RI setValue(const T *p, size_t count, RCode code) const;
private:
	bool m_isChanRef;
	union {
		Attribute *m_ra;
		Channel *m_rc;
	};
};

size_t ValueInterface::Dispatcher::getCount() const {
	if (m_isChanRef) return m_rc->flatCount();
	else return m_ra->count();
}

template <class T>
RI ValueInterface::Dispatcher::getValue(T *p,
										size_t index, size_t count) const {
	if (m_isChanRef) return m_rc->pim->getCurrentValue(p, index, count);
	else return m_ra->pim->getValue(p, index, count);
}

template <class T>
RI ValueInterface::Dispatcher::setValue(const T *p, size_t count,
										RCode code) const {
	bool codeAuto = code == rAuto;
	try {
		if (m_isChanRef) {
			if (!codeAuto && code != m_rc->representationCode())
				throw RI(RI::WrongCode);
			if (m_rc->flatCount() != count) throw RI(RI::WrongCount);
			return m_rc->pim->setCurrentValue(p);
		}
		else {
			Attribute::Impl &a = *m_ra->pim;
			const AttributeDefinition *pad = a.definition();
			if (!pad) {	// - если ограничений нет
				if (!codeAuto) return a.setValue(p, count, code);
			} else {	// - если ограничения есть
				if (!pad->validCount(count)) throw RI(RI::WrongCount);
				if (codeAuto) {
					vector<RCode> codes = pad->restricted.representationCodes;
					if (codes.size() != 0) {
					// Перебираем коды из списка ограничений
						for (uint n = 0; n < codes.size(); ++n) {
							RI ri = a.setValue(p, count, codes[n]);
							if (!ri.critical()) return ri;
						}
						throw RI (RI::WrongValType, 1);
						// - если ни один код не подошел, это считается ошибкой
					}
					// - Если codes.size()==0, код будет определен далее
					//	 по входному значению
				} else {
					if (!pad->validReprCode(code)) throw RI(RI::WrongCode);
					return a.setValue(p, count, code);
				} // if (codeAuto...)
			} // if (pad...)
				assert(codeAuto);
		// Определяем код на основе типа входного значения
				code = DlisIntTypes::Type<T>::valueBestCode(*p);
				if (code == rAuto) {
			code = Representation::typeBestCode<T>();
			if (code == rAuto) throw RI(RI::WrongValType, 2);
				}
			return a.setValue(p, count, code);
		} // if (m_isChanRef...)
	}
	catch(RI riGot) {
		return riGot.toCritical();
	}
}

ValueInterface::ValueInterface(const Dispatcher *vd) : m_pvd(vd) {}

ValueInterface::~ValueInterface() { delete m_pvd; }

size_t ValueInterface::count() const { return m_pvd->getCount(); }

#define GET_SET(type)										  \
RI ValueInterface::get(type *p, size_t pos, size_t cnt) const \
	{ return m_pvd->getValue(p, pos, cnt); }				  \
RI ValueInterface::set(const type *p, size_t cnt, RCode c)	  \
	{ return m_pvd->setValue(p, cnt, c); }
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
	GET_SET(DateTime)
#undef GET_SET

// ===========================================================================

class Item {
//protected:
//	virtual void notifyChanged(Item *i) {}
private:
	virtual void polymorphizerDummy() {}
};

class AttributeParent /*: public Item*/ {
public:
	virtual void notifyChanged(Item *i) {}
};

class Attribute::Impl : public Item {
public:
	class Component;
	class Comp_Interim : public Dlis::Component {
	private:
		friend class Component;
		Ident m_label;
		uint32 m_count;
		Representation::Code m_reprCode;
		Units m_units;
		SingleValue m_value;
	};
/* - NB: Вспомогательный промежуточный класс Comp_Interim, в который перенесены
		 члены-данные класса Component  введен только потому, что компилятор
		 MSVC 2013 не принимает в качестве аргумента шаблона вложенного класса
		 (Component::Tag) указатель на функцию-член внешнего класса (Component)
		 (т.е. фактически класса, объявление которого не завершено */
//	class Component : public Dlis::Component {		// Attribute Component

	class Component : public Comp_Interim    // Attribute Component 
    {		
// - NB: Класс Component описывается здесь, т.к. он нужен для задания типа
//		 поля m_c, которое (в целях эффективности) не является указателем
	public:
		template <typename T, T Comp_Interim:: *pm_,
				  uint fmtBitPos_, T (*getDefVal)() = NULL>
		class Tag {
		public:
			typedef T Type;
			Tag() : defVal(m_defVal), pm(pm_)
				{ if (getDefVal) m_defVal = getDefVal(); }
			const uint fmtBitPos = fmtBitPos_;
			const T &defVal;
/* - NB: Ссылка здесь используется как удобная альтернатива функции (размер
		 структуры Tag не имеет значения, т.к. ее экземпляры статические) */
		private:
			friend class Component;
			T Comp_Interim:: *const pm;
			T m_defVal;
/* - NB: Неконстантное поле m_defVal (и константная ссылка на него) введено
		 для того, чтобы его можно было устанавливать в коде конструктора
		 (в списке инициализации конструктора не удалось бы использовать
		 указатель на функцию getDefault со значением по умолчанию NULL) */
		};
/* - NB: Чтобы избавиться от промежуточного класса Comp_Interim, нужно
		 передавать указатель pm_ на функцию-член не через параметр шаблона,
		 а через конструктор (как в Channel::Impl::Tag) */
		static uint32 getDefCount()
			{ return 1; }
		static Representation::Code getDefReprCode()
			{ return Representation::IDENT; }
		typedef Tag<Ident, &Comp_Interim::m_label, 4> TagLabel;
		typedef Tag<uint32, &Comp_Interim::m_count, 3,
					&getDefCount> TagCount;
		typedef Tag<Representation::Code, &Comp_Interim::m_reprCode, 2,
					&getDefReprCode> TagReprCode;
		typedef Tag<Units, &Comp_Interim::m_units, 1> TagUnits;
		typedef Tag<SingleValue, &Comp_Interim::m_value, 0> TagValue;
		static const
		TagLabel &tLabel() { static TagLabel t; return t; }
		static const
		TagCount &tCount() { static TagCount t; return t; }
		static const
		TagReprCode &tReprCode() { static TagReprCode t; return t; }
		static const
		TagUnits &tUnits() { static TagUnits t; return t; }
		static const
		TagValue &tValue() { static TagValue t; return t; }
		// - Функции tXXX() возвращают (статические) теги доступа к полям
		//	 для использования их в шаблонных функциях
// - NB: Если сделать теги статическими полями, возникает ошибка линковщика
// - NB: В C++11 теги, вероятно, можно заменить с помощью кортежей (tuples)
		enum FormatBitPos 
        {
			bValue	  = 0,
			bUnits	  = 1,
			bReprCode = 2,	// Representation Code
			bCount	  = 3,
			bLabel	  = 4
		};
		// - Значения элементов перечисления FormatBitPos задают нoмера бит
		//	 в ComponentDescriptor.cf
		Component(const Ident &label) : m_rtplc(NULL) {
			m_d.role = rAttr;
			reset();
			if (!label.empty()) {
				m_label = label;
				m_d.fmt[bLabel] = true;
			}
			/* - NB: Используем пустую метку как признак ее отсутствия
					 (по стандарту DLIS метка атрибута не может быть пустой) */
		}
		/* - NB: Предполагается, что конструктор будет вызываться с непустым
				 параметром label для атрибутов в Set Template и с пустым -
				 для атрибутов объектов */
		// Шаблонная функция чтения характеристики компонента по ее тегу t:
		template <class Tag>
		const typename Tag::Type &charc(const Tag &t) const {
			const Component *c = !m_rtplc || m_d.fmt[t.fmtBitPos] ?
								 this : m_rtplc;
			return c->*t.pm;
		}
		const Ident &label() const			  { return charc(tLabel()); }
		uint32 count() const				  { return charc(tCount()); }
		Representation::Code reprCode() const { return charc(tReprCode()); }
		const Units &units() const			  { return charc(tUnits()); }
		const SingleValue &cvalue() const	  { return charc(tValue()); }
		void linkTemplate(const Component *pc) { m_rtplc = pc; }
	// Функции записи
		// Шаблонная функция изменения характеристики компонента по ее тегу t:
		template <class Tag>
		void setCharc(const Tag &t, const typename Tag::Type &val) {
			const typename Tag::Type &defVal = m_rtplc ?
											   m_rtplc->*t.pm : t.defVal;
			bool isDef = val == defVal;
			this->*t.pm = isDef ? t.defVal : val;
			m_d.fmt[t.fmtBitPos] = !isDef;
			// - Значение val устанавливается при совпадении со значением по
			//	 умолчанию в *m_rtplc (в Set Template) или с "global default",
			//	 в противном случае - сбрасывается
		}
//		template <class T>
//		RI setValue(const T &value) {
//			m_d.fmt[bValue] = true;
//			return m_value.set(value, reprCode());
//		}
//		template <class T>
//		RI setValue(const vector<T> &value) {
//			return setValue(&value[0], value.size());
//		}
		template <class T>
		RI setValue(const T *p, size_t count) {
			m_d.fmt[bValue] = true;
			return m_value.set(p, count, reprCode());
		}
		/* - NB: Для простоты сброс флага m_d.fmt[bValue] при присвоении
				 значений по умолчанию (например, пустых строк)
				 не поддерживается (кроме того, жесткий формат File Header)
				 требует установки этого флага у атрибута 'ID' даже при пустых
				 значениях) */
	protected:
			RoleGroup roleGroup() { return rgAttr; }
		void reset();
		void readCharacteristics(Input &input_file);
		void writeCharacteristics(Output &out) const;
	private:
//		Ident m_label;
//		uint32 m_count;
//		Representation::Code m_reprCode;
//		Units m_units;
//		SingleValue m_value;
		const Component *m_rtplc;
		// - Ссылка на компонент соответствующего атрибута из Set Template
		//	 (или NULL, если атрибут сам принадлежит Set Template)
	}; // class Component -----------------------------------------------------
	const AttributeDefinition *definition() const { return m_rdef; }
// Функции чтения
//	template <class T>
//	RI getValue(T &value, size_t index) const {
//		return component().cvalue().get(value, index);
//	}
//	template <class T>
//	RI getValue(vector<T> &value) const {
//		return component().cvalue().getAll(value);
//	}

	template <class T>
	RI getValue(T &value, size_t index = 0) const 
    {
		return getValue(&value, index, 1);
	}

	template <class T>
	RI getValue(vector<T> &value) const 
    {
		value.resize(component().count());
		return getValue(&value[0], 0, value.size());
	}

	template <class T>
	RI getValue(T *p, size_t index, size_t count) const 
    {
		return component().cvalue().get(p, index, count);
	}

	const Component &component() const { return m_c; }

	bool wrong() const { return m_wrong; }

	void markWrong() { m_wrong = true; }

	void read(Input &input_file) 
    { 
        m_c.read(input_file); 
    }

	void linkTemplate(const Attribute *pa) {
		m_c.linkTemplate(&pa->pim->component());
	}
// Функции записи
	void setUnits(const Units &units) {
		m_c.setCharc(m_c.tUnits(), units);
	}
	void setCount(uint32 count) {
		m_c.setCharc(m_c.tCount(), count);
	}
	void setRepresentationCode(Representation::Code code) {
		m_c.setCharc(m_c.tReprCode(), code);
	}
// - NB: Функции setCount и setRepresentationCode предназначены для атрибутов
//		 Set Template, для объектов следует пользоваться функцией setValue
	template <class T>
	RI setValue(const T &value,
				Representation::Code code = Representation::Auto) {
		return setValue(&value, 1, code);
	}
	// - NB: При задании значения value в виде литеральной строчной
	//		 константы, видимо, будет ошибка компиляции
	//		 (в шаблоне функции ValueInterface::set эта проблема решена )
	template <class T>
	RI setValue(const vector<T> &value,
				Representation::Code code = Representation::Auto) {
		return setValue(&value[0], value.size(), code);
	}
	template <class T>
	RI setValue(const T *p, size_t count, Representation::Code code) {
		Representation::Code bakCode = m_c.reprCode();
		if (code != Representation::Auto) setRepresentationCode(code);
		RI ri = m_c.setValue(p, count);
		if (ri.critical())
			setRepresentationCode(bakCode);
		else {
			setCount(count);
			m_rparent->notifyChanged(this);
		}
		return ri;
	}
	void write(Output &out) const { m_c.write(out); }
private:
	friend class Attribute;
	Impl(Attribute *pIntf, AttributeParent *parent,
		 const Ident &label, const AttributeDefinition *ad) :
		m_rparent(parent),
		m_c(label),
		m_rdef(ad),
		m_valIntf(new ValueInterface::Dispatcher(pIntf)),
		m_wrong(false) {}
	~Impl() {}
/* - NB: В деструкторе здесь вроде нет необходимости, но без него компилятор
		 выдает предупреждение:
			 "destructor could not be generated because a base class destructor
			 is inaccessible or deleted" */
	AttributeParent *const m_rparent;
	Component m_c;
	const AttributeDefinition *m_rdef;
	ValueInterface m_valIntf;
	bool m_wrong;
	// - Признак ошибки в атрибуте (только у атрибутов объектов в режиме чтения)
}; // class Attribute::Impl ===================================================

void Attribute::Impl::Component::reset() {
//	m_d.role = rAttr;
//	m_label.clear();
//	m_count = 1;
//	m_reprCode = Representation::IDENT;
//	m_units.clear();
//	m_value.clear();
	m_label =	 tLabel().defVal;
	m_count =	 tCount().defVal;
	m_reprCode = tReprCode().defVal;
	m_units =	 tUnits().defVal;
	m_value =	 tValue().defVal;
}

void Attribute::Impl::Component::readCharacteristics(Input &input_file) 
{

	using namespace Representation;

	FormatBits f = format();

	if (f[bLabel]) 
        SingleValue::read(m_label, input_file, IDENT);

	if (m_rtplc) 
    {
		bool valueNeeded = f[bCount] || f[bReprCode] || f[bUnits];

		if (valueNeeded && !f[bValue]) 
            throw RI(RI::BadAttr, 1);
	}

	if (f[bCount]) 
        SingleValue::read(m_count, input_file, UVARI);

	if (f[bReprCode]) 
        SingleValue::read(m_reprCode, input_file, USHORT);

	if (f[bUnits]) 
        SingleValue::read(m_units, input_file, IDENT);

	if (f[bValue]) 
        m_value.read(input_file, reprCode(), count());
	else 
        m_value.setNull(reprCode(), count());
}

void Attribute::Impl::Component::writeCharacteristics(Output &out) const {
	using namespace Representation;
	FormatBits f = format();
	if (f[bLabel]) SingleValue::write(out, m_label, IDENT);
	if (f[bCount]) SingleValue::write(out, m_count, UVARI);
	if (f[bReprCode]) SingleValue::write(out, m_reprCode, USHORT);
	if (f[bUnits]) SingleValue::write(out, m_units, IDENT);
	if (f[bValue]) m_value.write(out);
}


Attribute::Attribute(AttributeParent *parent,
					 const Ident &label, const AttributeDefinition *ad) :
	pim(new Impl(this, parent, label, ad)) {}

Attribute::~Attribute() {
	delete pim;
}

/*static*/ const string &Attribute::labelStrOf(const Attribute *pa)
	{ return pa->label().str(); }

const Ident &Attribute::label() const { return pim->component().label();}

uint32 Attribute::count() const { return pim->component().count(); }

Representation::Code Attribute::representationCode() const
	{ return pim->component().reprCode(); }

const Units &Attribute::units() const { return pim->component().units(); }

const ValueInterface &Attribute::cvalue() const { return pim->m_valIntf; }

ValueInterface &Attribute::value() { return pim->m_valIntf; }

//template <class T>
//RI Attribute::getValues(vector<T> &values) const
//	{ return pim->component().cvalue().getAll(values); }

//RI Attribute::setRepresentationCode(Representation::Code code) {
//	if (!Representation::valid(code)) return RI(RI::BadRepr, 1).toCritical();
//	pim->setRepresentationCode(code);
//	return RI();
//}

RI Attribute::setUnits(const Units &units) {
	if (!units.valid()) return RI(RI::BadUnits, 1).toCritical();
	pim->setUnits(units);
	return RI();
}
// end of class Attribute definitions

// ============================================================================

class ObjectParent /*: public Item*/ {
public:
	virtual void notifyChanged(Item *i) = 0;
	virtual bool addStdAttribute(const string &labelStr) = 0;
	virtual bool loadFramesToMemory() const = 0;
};

class BaseObjectImpl : public AttributeParent {
public:
	BaseObjectImpl(ObjectParent *parent) : m_rparent(parent) {}
	~BaseObjectImpl() { freeMem(); }
	void clear() { freeMem(); }
	ObjectParent *parent() { return m_rparent; }
	const vector<const Attribute *> &cattributes()
		{ return (vector<const Attribute *> &)mvp_attrs; }
	const uint attributeCount() const { return mvp_attrs.size(); }
	const Attribute *pcAttribute(uint index) const { return mvp_attrs[index]; }
	const Attribute *pcAttribute(const string &labelStr) const
		{ return findAttribute(labelStr); }
	const Attribute::Impl::Component &attributeComponent(uint index) const
		{ return mvp_attrs[index]->pim->component(); }
	virtual void write(Output &out) const;
	virtual void read(Input &input_file);
	Attribute *pAttribute(uint index) { return mvp_attrs[index]; }
	Attribute *pAttribute(const string &labelStr)
		{ return const_cast<Attribute *>(findAttribute(labelStr)); }
protected:
	const Attribute *findAttribute(const string &labelStr) const;
/* - NB: Класс BaseObjectImpl фактически является владельцем экземпляров
		 Attribute, доступных через контейнер mvp_attrs, и, чтобы не допустить
		 возможности их изменения, у возвращаемого значения установлен атрибут
		 const, хотя с формальной точки зрения можно было этого не делать */
// Функции для чтения
	virtual void readNextAttribute(Input &input_file, Attribute *pa, uint nAttr) = 0;
	virtual void checkAttributesOnRead() = 0;
// Функции для записи
	Attribute *addNewAttribute(const string &labelStr,
							   const AttributeDefinition *ad) {
		Attribute *pa = new Attribute(this, labelStr, ad);
		mvp_attrs.push_back(pa);
		return pa;
	}
// Data
	ObjectParent *const m_rparent;
	vector <Attribute *> mvp_attrs;
private:
	void freeMem() { clearPtrContainer(mvp_attrs, &Attribute::deletePtr); }
	void d_testAttrCompTagPrivacy() {
		Attribute::Impl::Component::TagCount tc;
		Attribute::Impl::Component c(Ident(""));
//		c.*tc.pm = 2;
	}
};

const Attribute *BaseObjectImpl::findAttribute(const string &labelStr) const {
	vector <Attribute *>::const_iterator ita;
//		vector <Attribute *>::iterator ita;
// - NB: Попытка присвоения результата findByKey неконстантному итератору
//		 почему-то вызывает ошибку компиляции
	ita = findByKey(mvp_attrs.begin(), mvp_attrs.end(),
					Attribute::labelStrOf, labelStr);
//	ita = findPtrByKey(mvp_attrs.begin(), mvp_attrs.end(),
//					   &Attribute::labelStr, labelStr);
	if (ita == mvp_attrs.end()) return NULL;
	else return *ita;
}

void BaseObjectImpl::read(Input &input_file) 
{
	freeMem();

	Component::Descriptor d;
	uint nAttr = 0;

	while (!input_file.atEnd()) 
    {
		std::streampos p = input_file.getPos();
		// Считываем далее идущий дескриптор для определения Component Role:
		d.read(input_file);
		// Возращаемся на позицию перед дескриптором:
		input_file.setPos(p);

		Component::RoleGroup rg = Component::roleGroupOf(d.role);

		if (rg == Component::rgObject) 
            break;

		if (rg != Component::rgAttr) 
            throw RI(RI::BadSet);

		Attribute *pa = new Attribute(this);
		try 
        {
			readNextAttribute(input_file, pa, nAttr++);
		}
		catch(...) 
        {
			delete pa; 
            throw;
		}

		mvp_attrs.push_back(pa);
	}

	checkAttributesOnRead();
}

void BaseObjectImpl::write(Output &out) const 
{
	for (uint n = 0; n < attributeCount(); ++n)
		pcAttribute(n)->pim->write(out);
}


class Template : public BaseObjectImpl 
{
public:
	Template(ObjectParent *parent) :
		BaseObjectImpl(parent),
		mm_rAttrDefs(NULL), m_nonInvAttrCnt(0) {}
	uint getNonInvariantAttributeCount() const { return m_nonInvAttrCnt; }
	/* - NB: Предполагается, что инвариантные атрибуты расположены в конце
			 списка атрибутов: в режиме чтения они переносятся в конец списка
			 по окончании чтения Set Template (см. функцию read), а в режиме
			 записи должны сразу добавляться в конец списка (не реализовано,
			 т.к. инвариантные атрибуты в режиме записи пока
			 не поддерживаются) */
	void initAttributes(const AttributeMap *am);
/* - NB: Если считать, что определения атрибутов (am) нужны и для
		 режима чтения (хотя пока не используются), то, с учетом этого,
		 их затруднительно передавать через конструктор */
	void read(Input &input_file);
// Функции для записи
	Attribute *addNewAttribute(const string &labelStr);

protected:
	void readNextAttribute(Input &input_file, Attribute *pa, uint nAttr);
	void checkAttributesOnRead();

private:
	const AttributeMap *mm_rAttrDefs;
	uint m_nonInvAttrCnt;	// Количество неинвариантных атрибутов

}; // class Template
/* - NB: Наследованная от BaseObjectImpl неконстантная функция pAttribute
		 в классе Template оставлена с protected-доступом, т.к. поддержка
		 возможности изменения атрибутов Set Template (которые выполняют роль
		 атрибутов по умолчанию для атрибутов объектов) значительно
		 усложнила бы программу */

void Template::read(Input &input_file) 
{
	BaseObjectImpl::read(input_file);

// Переносим инвариантные атрибуты в конец списка
	uint attrCnt = mvp_attrs.size();

	m_nonInvAttrCnt = attrCnt;
	uint n = 0;

	while (n < attrCnt) 
    {
	// - NB: Для простого переноса элементов достаточно условия n<attrCnt-1,
	//		 однако для определения m_nonInvAttrCnt нужно пройти все элементы
		Attribute::Impl::Component ac = attributeComponent(n);
		if (ac.role() == Component::rInvarAttr) 
        {
			Attribute *pa = mvp_attrs[n];

			mvp_attrs.erase(mvp_attrs.begin() + n);
			mvp_attrs.push_back(pa);
			--m_nonInvAttrCnt;
		}
		else
			++n;
	}
}

void Template::initAttributes(const AttributeMap *am) {
	if (!am) return;
		if (mm_rAttrDefs) throw RI(RI::ProgErr, 8);
	mm_rAttrDefs = am;
// Добавляем обязательные атрибуты в Set Template
	for (uint n = 0; n < am->count(); ++n) {
		const AttributeMap::DefinitionPair &adp = am->definitionPair(n);
		if (adp.second.tagged) addNewAttribute(adp.first);
	}
}

void Template::readNextAttribute(Input &input_file, Attribute *pa, uint nAttr) {
	pa->pim->read(input_file);
	// - NB: Проверяется только формат, а "содержательный" контроль
	//		 введенных атрибутов выполняется в checkAttributesOnRead()
}

void Template::checkAttributesOnRead() {
	for (unsigned n = 0; n < mvp_attrs.size(); ++n) {
		Attribute::Impl::Component ac = attributeComponent(n);
		bool ok = !ac.label().empty() && ac.role() != Component::rAbsentAttr;
		if (!ok) throw RI(RI::BadTpl, 1);
	}
	if (mvp_attrs.size() > 1) {
		bool attrsUnique = allUniqueKeys(mvp_attrs.begin(), mvp_attrs.end(),
										 Attribute::labelStrOf);
// - NB: В C++11 Attribute::Impl::labelStrOf можно было бы заменить
//		 на лямбда-функцию
		if (!attrsUnique) throw RI(RI::BadTpl, 2);
	}
}

Attribute *Template::addNewAttribute(const string &labelStr) {
		assert(!labelStr.empty());
	if (pAttribute(labelStr))
		return NULL;	// - атрибут уже присутствует в Set Template
	const AttributeDefinition *pad =
			mm_rAttrDefs ? mm_rAttrDefs->definition(labelStr) : NULL;
	Attribute *pa = BaseObjectImpl::addNewAttribute(labelStr, pad);
	++m_nonInvAttrCnt;
	if (pad) {
		uint vcnt = pad->restricted.valueCount;
		if (vcnt != -1) pa->pim->setCount(vcnt);
		const vector<Representation::Code> &codes =
				pad->restricted.representationCodes;
		if (codes.size() == 1) pa->pim->setRepresentationCode(codes[0]);
	}
	return pa;
}

// ============================================================================

class Object::Impl : public BaseObjectImpl, public Item {
public:
	class Component : public Dlis::Component {
	public:
//		Component() { reset(); }
		Component(const ObjectName &name) {
			m_d.role = rObject;
			m_d.fmt[bName] = true;
			m_n = name;
		}
		const ObjectName &name() { return m_n; }
	protected:
			RoleGroup roleGroup() { return rgObject; };
		void reset() {
			m_n.clear();
		}
		void readCharacteristics(Input &input_file) 
        {
			FormatBits f = format();

			for (int n = 0; n < bName; ++n)
				if (f[n]) throw RI(RI::BadCompDescr);

			if (f[bName]) 
                SingleValue::read(m_n, input_file, Representation::OBNAME);

			if (m_n.ident().empty()) 
                input_file.addIssue(RI::NoObjName);
		}

		void writeCharacteristics(Output &out) const {
			FormatBits f = format();
			if (f[bName]) SingleValue::write(out, m_n, Representation::OBNAME);
		}
	private:
		enum FormatBitPos 
        {
			/* Reserved bits, must be 0 */
			bName = 4
		};

		// - см. примечание к классу Attribute::Impl::Component
		ObjectName m_n;
	}; // class Component
// ----------------------------------------------------------------------------
	static const AttributeMap *pAttributeMap(Object::Type type);
	// - Таблицы атрибутов тех DLIS-объектов (типа type), которые
	//	 не представлены собственными классами
	static Object::Type type(const string &typeId)
		{ return getTypeTable().type(typeId); }
	static const string &typeId(Object::Type type)
		{ return getTypeTable().typeId(type); }
	static bool typeIsCertain(Object::Type type)
		{ return type >= Object::FILE_HEADER && type < Object::TypeOther; }
/* - NB: Здесь учитывается возможность появления новых (вспомогательных)
		 элементов в начале списка значений перечислимого типа Object::Type
		 и добавления элементов перед TypeOther для еще не внесенных типов
		 DLIS-объектов  */
//	static const AttributeDefinitions &attributeDefinitions(Object::Type &type);
// ----------------------------------------------------------------------------
//	Impl() :
//		m_rtypeid(NULL), m_rtpl(NULL) {}
//	Impl(const ObjectName &name) :
//		m_rtypeid(NULL), m_rtpl(NULL) { m_c.name = name; }
	Impl(ObjectParent *parent, const ObjectName &name) :
		BaseObjectImpl(parent),
		m_c(name), m_rtypeid(NULL), m_rtpl(NULL) {}
	void initialize(const string *objectTypeId, const Template *tpl,
					bool createAttributes);
	// - Предполагается, что initialize вызывается сразу после конструктора
// Функции для чтения
	void read(Input &input_file);
// Функции для записи
	void completeAttributes();
	// - Синхронизирует список атрибутов с Set Template (в *m_rtpl), добавляя
	//	 отсутствующие атрибуты,
	template <class Tag>
	void setAttribute(typename Tag::Owner *owner,
					  const Tag &t, const typename Tag::Type &value) {
		owner->*t.pm = value;
		storeAttribute(owner, t);
//		if (t.afterRestore) (owner->*t.afterRestore)();
			restoreAttribute(owner, t);
// - NB: Хотя здесь достаточно было бы вызова afterRestore, выполняем
//		 для надежности restoreAttribute (DEBUG)
	}
	void write(Output &out) const;
	using BaseObjectImpl::pAttribute;
protected:
	template <class AttrOwner, typename T> class AttributeTag {
	public:
		typedef AttrOwner Owner;
		typedef T Type;
		AttributeTag(uint indexAttrDef, T Owner:: *pm,
					 void (Owner:: *afterRestore)() = NULL) :
			pm(pm),
			afterRestore(afterRestore),
			m_padp(&Owner::pAttributeMap()->definitionPair(indexAttrDef)) {}
		bool match(const Attribute::Impl *a) const
			{ return a->definition() == &m_padp->second; }
		const string &label() const { return m_padp->first; }
		T Owner::*const pm;
		void (Owner:: *const afterRestore)();
	private:
//		friend Owner;
/* - NB: Это объявление не сработало так, как ожидалось, поэтому поля
		 pm и afterRestore пришлось объявить как public.
		 Но теперь экземпляры AttributeTag нельзя использовать за пределами
		 ветви классов Object (даже с атрибутом const) */
		const AttributeMap::DefinitionPair *m_padp;
	}; // template class AttributeTag
// ----------------------------------------------------------------------------
	void readNextAttribute(Input &input_file, Attribute *pa, uint nAttr);
	void checkAttributesOnRead();
	template <class Tag>
		bool hasAttribute(const Tag &t) const { return pcAttribute(t.label()); }
	template <class TagX>
	bool restoreAttribute(typename TagX::Owner *owner, const TagX &t);
	template <class TagX>
	void storeAttribute(typename TagX::Owner *owner, const TagX &t);
	template <class TagX>
	bool handleChanged(typename TagX::Owner *owner,
						const TagX &t, const Attribute::Impl *a) {
		bool match = t.match(a);
		if (match) restoreAttribute(owner, t);
		return match;
	}
private:
	friend class Object;
	class TypeTable {
	public:
		TypeTable() {
			for (int n = 0; n < Object::TypeOther; ++n)
				m_map.insert(std::make_pair(m_ids[n], (Object::Type)n));
		}
		Object::Type type(const string &typeId) const {
			Map::const_iterator it = m_map.find(typeId);
			return it != m_map.end() ? it->second : Object::TypeOther;
		}
		const string &typeId(Object::Type type) { return m_ids[type]; }
	private:
		typedef std::map<string, Object::Type> Map;
		Map m_map;
		static const string m_ids[];
	};
// ----------------------------------------------------------------------------
	static TypeTable &getTypeTable() { static TypeTable t; return t; }
	RI adjustToTemplate();
// Data -----------------------------------------------------------------------
	Component m_c;
	const string *m_rtypeid;  // Ссылка на идентификатор типа объекта (в Set)
	const Template *m_rtpl;	  // Ссылка на Set Template
}; // class Object::Impl ======================================================


const string Object::Impl::TypeTable::m_ids[] = {
	"",					// Object::typeUndefined
	"FILE-HEADER",		// Object::FILE_HEADER
	"ORIGIN",			//	...
	"WELL-REFERENCE-POINT",
	"AXIS",
	"CHANNEL",
	"FRAME",
	"PATH",
	"CALIBRATION",
	"CALIBRATION-COEFFICIENT",
	"CALIBRATION-MEASUREMENT",
	"COMPUTATION",
	"EQUIPMENT",
	"GROUP",
	"PARAMETER",
	"PROCESS",
	"SPLICE",
	"TOOL",
	"ZONE",
	"COMMENT",
	"MESSAGE",
//	"UPDATE",			//  ...
	""					// Object::TypeOther
};
// - NB: В C++11 проинициализировать typeIds можно внутри определения класса

/*static*/ const AttributeMap *Object::Impl::pAttributeMap(Object::Type type) {
	using namespace Representation;
	switch (type) {
	case Object::ORIGIN: {
		static AttributeMap am;
		am.add("FILE-ID",			 1, ASCII);
		am.add("FILE-SET-NAME",		 1, IDENT);
		am.add("FILE-SET-NUMBER",	 1, UVARI);
		am.add("FILE-NUMBER",		 1, UVARI);
		am.add("FILE-TYPE",			 1, IDENT);
		am.add("PRODUCT",			 1, ASCII);
		am.add("VERSION",			 1, ASCII);
		am.add("PROGRAMS",				ASCII);
		am.add("CREATION-TIME",		 1, DTIME);
		am.add("ORDER-NUMBER",		 1, ASCII);
		am.add("DESCENT-NUMBER"				 );
		am.add("RUN-NUMBER"					 );
		am.add("WELL-ID",			 1		 );
		am.add("WELL-NAME",			 1, ASCII);
		am.add("FIELD-NAME",		 1, ASCII);
		am.add("PRODUCER-CODE",		 1, UNORM);
		am.add("PRODUCER-NAME",		 1, ASCII);
		am.add("COMPANY",			 1, ASCII);
		am.add("NAME-SPACE-NAME",	 1, IDENT);
		am.add("NAME-SPACE-VERSION", 1, UVARI);
		return &am;
	}
	case Object::WELL_REFERENCE_POINT: {
		static AttributeMap am;
		am.add("PERMANENT-DATUM",			1, ASCII);
		am.add("VERTICAL-ZERO",				1, ASCII);
		am.add("PERMANENT-DATUM-ELEVATION",	1       );
		am.add("ABOVE-PERMANENT-DATUM",		1       );
		am.add("MAGNETIC-DECLINATION",		1       );
		am.add("COORDINATE-1-NAME",			1, ASCII);
		am.add("COORDINATE-1-VALUE",		1       );
		am.add("COORDINATE-2-NAME",			1, ASCII);
		am.add("COORDINATE-2-VALUE",		1       );
		am.add("COORDINATE-3-NAME",			1, ASCII);
		am.add("COORDINATE-3-VALUE",		1       );
		return &am;
	}
	case Object::AXIS: {
		static AttributeMap am;
		am.add("AXIS-ID",	  1, IDENT);
		am.add("COORDINATES"		  );
		am.add("SPACING",	  1		  );
		return &am;
	}
	case Object::PATH: {
		static AttributeMap am;
		am.add("FRAME-TYPE",		   1, OBNAME);
		am.add("WELL-REFERENCE-POINT", 1, OBNAME);
		am.add("VALUE",					  OBNAME);
		am.add("BOREHOLE-DEPTH",	   1		);
		am.add("VERTICAL-DEPTH",	   1		);
		am.add("RADIAL-DRIFT",		   1		);
		am.add("ANGULAR-DRIFT",		   1		);
		am.add("TIME",				   1		);
		am.add("DEPTH-OFFSET",		   1		);
		am.add("MEASURE-POINT-OFFSET", 1		);
		am.add("TOOL-ZERO-OFFSET",	   1		);
		return &am;
	}
	case Object::CALIBRATION: {
		static AttributeMap am;
		am.add("CALIBRATED-CHANNELS",	   OBNAME);
		am.add("UNCALIBRATED-CHANNELS",	   OBNAME);
		am.add("COEFFICIENTS",			   OBNAME);
		am.add("MEASUREMENTS",			   OBNAME);
		am.add("PARAMETERS",			   OBNAME);
		am.add("METHOD",				1, IDENT );
		return &am;
	}
	case Object::CALIBRATION_COEFFICIENT: {
		static AttributeMap am;
		am.add("LABEL",			   1, IDENT);
		am.add("COEFFICIENTS"			   );
		am.add("REFERENCES"				   );
		am.add("PLUS-TOLERANCES"		   );
		am.add("MINUS-TOLERANCES"		   );
		return &am;
	}
	case Object::CALIBRATION_MEASUREMENT: {
		static AttributeMap am;
		am.add("PHASE",				 1, IDENT );
		am.add("MEASUREMENT-SOURCE", 1, OBJREF);
		am.add("TYPE",				 1, IDENT );
		am.add("DIMENSION",				UVARI );
		am.add("AXIS",					OBNAME);
		am.add("MEASUREMENT"				  );
		am.add("SAMPLE-COUNT",		 1        );
		am.add("MAXIMUM-DEVIATION",	 1        );
		am.add("STANDARD-DEVIATION", 1        );
		am.add("BEGIN-TIME",		 1        );
		am.add("DURATION",			 1        );
		am.add("REFERENCE"					  );
		am.add("STANDARD"					  );
		am.add("PLUS-TOLERANCE"				  );
		am.add("MINUS-TOLERANCE"			  );
		return &am;
	}
	case Object::COMPUTATION: {
		static AttributeMap am;
		am.add("LONG-NAME",	 1, OBNAME, ASCII);
		am.add("PROPERTIES",	IDENT		 );
		am.add("DIMENSION",		UVARI		 );
		am.add("AXIS",			OBNAME		 );
		am.add("ZONES",			OBNAME		 );
		am.add("VALUES"						 );
		am.add("SOURCE"						 );
		return &am;
	}
	case Object::EQUIPMENT: {
		static AttributeMap am;
		am.add("TRADEMARK-NAME",   1, ASCII );
		am.add("STATUS",		   1, STATUS);
		am.add("TYPE",			   1, IDENT );
		am.add("SERIAL-NUMBER",	   1, IDENT );
		am.add("LOCATION",		   1, IDENT );
		am.add("HEIGHT",		   1        );
		am.add("LENGTH",		   1        );
		am.add("MINIMUM-DIAMETER", 1        );
		am.add("MAXIMUM-DIAMETER", 1        );
		am.add("VOLUME",		   1        );
		am.add("WEIGHT",		   1        );
		am.add("HOLE-SIZE",		   1        );
		am.add("PRESSURE",		   1        );
		am.add("TEMPERATURE",	   1        );
		am.add("VERTICAL-DEPTH",   1        );
		am.add("RADIAL-DRIFT",	   1        );
		am.add("ANGULAR-DRIFT",	   1        );
		return &am;
	}
	case Object::GROUP: {
		static AttributeMap am;
		am.add("DESCRIPTION", 1, ASCII		   );
		am.add("OBJECT-TYPE", 1, IDENT		   );
		am.add("OBJECT-LIST",    OBNAME, OBJREF);
		am.add("GROUP-LIST",     OBNAME		   );
		return &am;
	}
	case Object::PARAMETER: {
		static AttributeMap am;
		am.add("LONG-NAME",	1, OBNAME, ASCII);
		am.add("DIMENSION",	   UVARI		);
		am.add("AXIS",		   OBNAME		);
		am.add("ZONES",		   OBNAME		);
		am.add("VALUES"						);
		return &am;
	}
	case Object::PROCESS: {
		static AttributeMap am;
		am.add("DESCRIPTION",		  1, OBNAME, ASCII);
		am.add("TRADEMARK-NAME",	  1, ASCII		  );
		am.add("VERSION",			  1, ASCII		  );
		am.add("PROPERTIES",			 IDENT		  );
		am.add("STATUS",			  1, IDENT		  );
		am.add("INPUT-CHANNELS",		 OBNAME		  );
		am.add("OUTPUT-CHANNELS",		 OBNAME		  );
		am.add("INPUT-COMPUTATIONS", 	 OBNAME		  );
		am.add("OUTPUT-COMPUTATIONS",	 OBNAME		  );
		am.add("PARAMETERS",			 OBNAME		  );
		am.add("COMMENTS",				 ASCII		  );
		return &am;
	}
	case Object::SPLICE: {
		static AttributeMap am;
		am.add("OUTPUT-CHANNEL", 1, OBNAME);
		am.add("INPUT-CHANNELS",    OBNAME);
		am.add("ZONES",			    OBNAME);
		return &am;
	}
	case Object::TOOL: {
		static AttributeMap am;
		am.add("DESCRIPTION",	 1, ASCII );
		am.add("TRADEMARK-NAME", 1, ASCII );
		am.add("GENERIC-NAME",	 1, ASCII );
		am.add("PARTS",				OBNAME);
		am.add("STATUS",		 1, STATUS);
		am.add("CHANNELS",			OBNAME);
		am.add("PARAMETERS",		OBNAME);
		return &am;
	}
	case Object::ZONE: {
		static AttributeMap am;
		am.add("DESCRIPTION", 1, ASCII);
		am.add("DOMAIN",	  1, IDENT);
		am.add("MAXIMUM",	  1		  );
		am.add("MINIMUM",	  1		  );
		return &am;
	}
	case Object::COMMENT: {
		static AttributeMap am;
		am.add("TEXT", ASCII);
		return &am;
	}
	case Object::MESSAGE: {
		static AttributeMap am;
		am.add("TYPE",			 1, IDENT);
		am.add("TIME",			 1       );
		am.add("BOREHOLE-DEPTH", 1       );
		am.add("VERTICAL-DEPTH", 1       );
		am.add("RADIAL-DRIFT",	 1       );
		am.add("ANGULAR-DRIFT",	 1       );
		am.add("TEXT",				ASCII);
		return &am;
	}
//	case Object::XXX: {
//		static AttributeMap am;
//		am.add("",			 1, ASCII);
//		return &am;
//	}
	default:
		return NULL;
	}
} // Object::Impl::pAttributeMap

void Object::Impl::initialize(const string *objectTypeId, const Template *tpl,
							  bool createAttributes) {
	m_rtypeid = objectTypeId;
	m_rtpl = tpl;
	if (createAttributes) completeAttributes();
}

void Object::Impl::completeAttributes() {
	uint tplAttrCnt = m_rtpl->getNonInvariantAttributeCount();
	for (uint n = attributeCount(); n < tplAttrCnt; ++n) {
		const Attribute *paTpl = m_rtpl->pcAttribute(n);
		const AttributeDefinition *pad = paTpl->pim->definition();
		addNewAttribute("", pad)->pim->linkTemplate(paTpl);
	}
}

template <class TagX>
bool Object::Impl::restoreAttribute(typename TagX::Owner *owner,
									const TagX &t) {
	const Attribute *pa = pcAttribute(t.label());
	if (!pa) return true;
	// - NB: Наличие обязательных атрибутов проверяется в других местах
	//		 (в основном в функции parse)
	const AttributeDefinition *pad = pa->pim->definition();
	// - pad!=NULL только при записи
	if (!pad) pad = owner->pAttributeMap()->definition(t.label());
	// - в режиме чтения обращаемся непосредственно к таблице определений
	//	 атрибутов (это медленнее, т.к. выполняется поиск)
	bool ok = pad->validCount(pa->count()) &&
			  pad->validReprCode(pa->representationCode()) &&
//			  pa->pim->getValue(this->*t.pm).ok();
				  pa->cvalue().get(owner->*t.pm).ok();
/* - NB: Вместо более короткого пути чтения значения атрибута через функцию
		 Attribute::Impl::getValue используем функцию ValueInterface::get из
		 внешнего интерфейса модуля в целях ее дополнительного тестирования
		 (DEBUG) */
	if (!ok) return false;
	if (t.afterRestore) (owner->*t.afterRestore)();
	return true;
}

template <class TagX>
void Object::Impl::storeAttribute(typename TagX::Owner *owner, const TagX &t) {
	pAttribute(t.label())->pim->setValue(owner->*t.pm);
}
// - NB: Использование идентификатора Tag в качестве параметра шаблона функции
//		 (в точном соответствии с его объявлением) вызывало ошибку компиляции


void Object::Impl::read(Input &input_file) 
{
	m_c.read(input_file);
	// - NB: Первой во входном потоке здесь должна оказаться
	//		 Object-компонента, т.к. чтение в предшествующей операции
	//		  (Template::read) заканчивается именно ее обнаружением
	BaseObjectImpl::read(input_file);

	RI ri = adjustToTemplate();

	if (!ri.ok()) 
        input_file.addIssue(ri);

    // Удаляем Absent-атрибуты, а также атрибуты, отмеченные как некорректные
	for (int n = mvp_attrs.size() - 1; n >= 0; --n) 
    {
		Attribute *pa = mvp_attrs[n];

		bool absent = pa->pim->component().role() == Component::rAbsentAttr;

		if (absent || pa->pim->wrong()) 
            mvp_attrs.erase(mvp_attrs.begin() + n);
	}
         /* - NB: С этого момента индексы атрибутов в Set Template и самом объекте могут
		 отличаться (поэтому следует пользоваться исключительно ссылками
		 Attribute::Impl::Component::m_rtplc) */
}

void Object::Impl::readNextAttribute(Input &input_file, Attribute *pa, uint nAttr) {
	try {
		pa->pim->linkTemplate(m_rtpl->pcAttribute(nAttr));
		pa->pim->read(input_file);
		// - см. комментарий в Template::readNextAttribute()
	}
	catch(RI ri) {
// - NB: переменная ri нужна только для контроля при отладке
		input_file.addIssue(RI::WrongAttr);
		pa->pim->markWrong();
		/* - NB: Некорректные атрибуты помечаются и затем будут удалены */
	}
}

void Object::Impl::checkAttributesOnRead() {
	for (unsigned n = 0; n < mvp_attrs.size(); ++n) {
		Attribute::Impl::Component ac = attributeComponent(n);
		if (ac.role() == Component::rInvarAttr)
			throw RI(RI::BadAttr, 2);
	}
}

RI Object::Impl::adjustToTemplate() 
{
	RI   ri;
	uint nonInvAttrCnt = m_rtpl->getNonInvariantAttributeCount();

	if (mvp_attrs.size() > nonInvAttrCnt) 
    {
		ri = RI(RI::ManyObjAttrs);
		erasePtrContainer(mvp_attrs, mvp_attrs.end() - nonInvAttrCnt, mvp_attrs.end());
		// - Удаляем лишние атрибуты
	}
	// - NB: Не выполняем проверку mvp_attrs.size() < nonInvAttrCnt,
	//		 т.к. последние атрибуты в списке могут быть опущены
	for (uint n = mvp_attrs.size(); n < m_rtpl->attributeCount(); ++n) 
    {
		Attribute *pa = new Attribute(this);

		pa->pim->linkTemplate(m_rtpl->pcAttribute(n));
		mvp_attrs.push_back(pa);
	}
	/* - Для удобства, чтобы объект имел полный комплект атрибутов, как
		 в Set Template (за исключением Absent-атрибутов), добавляем к списку
		 атрибутов объекта пустые атрибуты (c характеристиками по умолчанию) -
		 как на месте опущенных, так и соответствующие инвариантным атрибутам
		 из Set Template */
	return ri;
}

void Object::Impl::write(Output &out) const {
	m_c.write(out);
	BaseObjectImpl::write(out);
}


//Object::Object() : pim(new Impl()) {}

Object::Object(ObjectParent *parent, const ObjectName &name) :
	pim(new Impl(parent, name)) {}

Object::~Object() {
	delete pim;
// - NB: Надо проанализировать предупреждение компилятора на эту инструкцию
}

/*static*/ const string &Object::typeId(Object::Type type)
	{ return Object::Impl::typeId(type); }

/*static*/ const ObjectName &Object::nameOf(const Object *po)
	{ return po->name(); }

/*static*/ ObjectReference Object::referenceTo(const Object *po)
	{ return ObjectReference(po->typeId(), po->name()); }

const ObjectName &Object::name() const { return pim->m_c.name(); }

Object::Type Object::type() const { return Impl::type(typeId()); }

const string &Object::typeId() const { return *pim->m_rtypeid; }

unsigned Object::attributeCount() const { return pim->mvp_attrs.size(); }

Object::AttributeConstIt Object::cbeginAttribute() const {
	return pim->cattributes().begin();
}

Object::AttributeConstIt Object::cendAttribute() const {
	return pim->cattributes().end();
}

const Attribute *Object::cattribute(const string &labelStr) const {
	AttributeConstIt ita;
	ita = findByKey(cbeginAttribute(), cendAttribute(),
					Attribute::labelStrOf, labelStr);
	if (ita == cendAttribute()) return NULL;
	else return *ita;
}

Attribute *Object::attribute(const string &labelStr) {
	return const_cast<Attribute *>(cattribute(labelStr));
}

Attribute *Object::stdAttribute(const string &labelStr) {
	if (pim->m_rparent->addStdAttribute(labelStr))
	// - Запрашиваем добавление атрибута
		return attribute(labelStr);
	else
		return NULL;
}

// ============================================================================

class FileHeader : public Object {
public:
	static Object::Type type() { return Object::FILE_HEADER; }
// Фиксированные значения длины атрибутов
	static const uint seqNumLength = 10;	// - атрибут "SEQUENCE-NUMBER"
	static const uint idLength = 65;		// - атрибут "ID"
// Таблица атрибутов
	static const AttributeMap *pAttributeMap();
// ----------------------------------------------------------------------------
	FileHeader(ObjectParent *parent, const ObjectName &name) :
		Object(parent, name) {}
	RI parse(uint32 &seqNum, string &id) const;
	void setSeqNum(uint32 seqNum);
	void setId(const string &id);
private:
//	const Attribute::Impl::Component
//		*getValidAttrComp(const string &attrLabelStr) const;
	const Attribute *getValidAttr(const string &attrLabelStr) const;
};

/*static*/ const AttributeMap *FileHeader::pAttributeMap() {
	static AttributeMap am;
	using namespace Representation;
	am.tagged = true;
	am.add("SEQUENCE-NUMBER", 1, ASCII);
	am.add("ID",			  1, ASCII);
	return &am;
}

void FileHeader::setSeqNum(uint32 seqNum)
	{ pim->pAttribute(0)->pim->setValue(intToStr(seqNum, seqNumLength)); }

void FileHeader::setId(const string &id) {
	string idVal;
	if (id.length() >= idLength) idVal = string(id, 0, idLength);
	else idVal = id + string(idLength - id.length(), ' ');
	pim->pAttribute(1)->pim->setValue(idVal);
}

//RI FileHeader::parse(uint32 &seqNum, string &id) const {
//	RI ri;
//	const Attribute::Impl::Component *pac;
//	pac = getValidAttrComp("SEQUENCE-NUMBER");
//	if (!pac) return RI(RI::BadFH, 1);
//	string s;
//	pac->cvalue().get(s);
//	long sn;
//	if (!strToInt(s, sn) || sn < 0) {
//		seqNum = 0;
//		return RI(RI::BadFH, 2);
//	}
//	else
//		seqNum = sn;
//	if (s.length() != seqNumLength || seqNum == 0) ri = RI(RI::NonStdFH, 1);
//	pac = getValidAttrComp("ID");
//	if (!pac) return RI(RI::BadFH, 3);
//	pac->cvalue().get(id);
//	if (id.length() != idLength) ri = RI(RI::NonStdFH, 2);
//	eraseTrailingSpaces(id);
//	if (pim->pcAttribute(1)->label().str() != "ID") ri = RI(RI::NonStdFH, 3);
//	// - проверка стандартного порядка атрибутов
//	return ri;
//}

//const Attribute::Impl::Component
//  *FileHeader::getValidAttrComp(const string &attrLabelStr) const {
//	const Attribute *pa = pim->pcAttribute(attrLabelStr);
//	if (!pa || pa->pim->wrong()) return NULL;
//	const Attribute::Impl::Component &ac = pa->pim->component();
//	bool ok = ac.count() == 1 && ac.reprCode() == Representation::ASCII &&
//			  ac.units().empty();
//	if (!ok) return NULL;
//	return &ac;
//}

RI FileHeader::parse(uint32 &seqNum, string &id) const {
	RI ri;
	const Attribute *pa;
	pa = getValidAttr("SEQUENCE-NUMBER");
	if (!pa) return RI(RI::BadFH, 1).toCritical();
	string s;
	pa->pim->getValue(s);
	long sn;
	if (!strToInt(s, sn) || sn < 0) {
		seqNum = 0;
		return RI(RI::BadFH, 2).toCritical();
	}
	else
		seqNum = sn;
	if (s.length() != seqNumLength || seqNum == 0) ri = RI(RI::NonStdFH, 1);
	pa = getValidAttr("ID");
	if (!pa) return RI(RI::BadFH, 3).toCritical();
	pa->pim->getValue(id);
	if (id.length() != idLength) ri = RI(RI::NonStdFH, 2);
	eraseTrailingSpaces(id);
	if (pim->pcAttribute(1)->label().str() != "ID") ri = RI(RI::NonStdFH, 3);
	// - проверка стандартного порядка атрибутов
	return ri;
}
/* - NB: Правильнее было бы реализовать parse() в этом классе по общей схеме,
		 в т.ч. с учетом описания AttributeDefinition
		 (см. например, реализацию функции Channel::Impl::parse) */

const Attribute
*FileHeader::getValidAttr(const string &attrLabelStr) const {
	const Attribute *pa = pim->pcAttribute(attrLabelStr);
	if (!pa || pa->pim->wrong()) return NULL;
	const Attribute::Impl::Component &ac = pa->pim->component();
	bool ok = ac.count() == 1 && ac.reprCode() == Representation::ASCII &&
			  ac.units().empty();
	if (!ok) return NULL;
	return pa;
}
// end of FileHeader class definitions

// ============================================================================

Origin::Origin(ObjectParent *parent, const ObjectName &name) :
	Object(parent, name) {}

// ============================================================================

class Channel::Impl : public Object::Impl {
public:
	static const AttributeMap *pAttributeMap();
	long valueSize() const {	// Размер значения в байтах
		uint elsz = Representation::getSize(m_rc);
		return elsz == -1 ? -1 : m_flatCnt * elsz;
	}
	bool frameable() const;
	// - Проверка готовности данного CHANNEL-объекта для его использования
	//	 в FRAME-объекте (наличие атрибутов REPRESENTATION-CODE, DIMENSION
	//	 и ELEMENT-LIMIT
	void setNullCurrentValue() { m_val.setNull(m_rc, m_flatCnt); }
//	template <class T>
//		RI getCurrentValue(T &value, size_t flatIndex) const;
//	template <class T>
//		RI getCurrentValue(vector<T> &value) const;
	template <class T>
		RI getCurrentValue(T *p, size_t flatIndex, size_t count) const;
//	template <class T>
//		RI setCurrentValue(const T &value);
	template <class T>
		RI setCurrentValue(const T *p);
	void read(Input &input_file);
//	void readCurrentValue(const byte *&p);	// - пока не нужно
	void readCurrentValue(Input &input_file);
	void writeCurrentValue(Output &out) const;
private:
	friend class Channel;
	typedef AttributeTag<Impl, Representation::Code> TagReprCode;
	typedef AttributeTag<Impl, Units>				 TagUnits;
	typedef AttributeTag<Impl, vector<uint32> >		 TagDimension;
	typedef AttributeTag<Impl, vector<uint32> >		 TagElemLimit;
	static const TagReprCode &tReprCode() {
		static TagReprCode t(0, &Impl::m_rc);
		return t;
	}
	static const TagUnits &tUnits() {
		static TagUnits t(1, &Impl::m_u);
		return t;
	}
	static const TagDimension &tDimension() {
		static TagDimension t(2, &Impl::mv_dim, &Impl::afterRestoreDimension);
		return t;
	}
	static const TagElemLimit &tElemLimit() {
		static TagElemLimit t(3, &Impl::mv_ellim);
		return t;
	}
// ----------------------------------------------------------------------------
	Impl(Channel *pIntf, ObjectParent *parent, const ObjectName &name) :
		Object::Impl(parent, name),
		m_rc(Representation::Undefined),
		m_valIntf(new ValueInterface::Dispatcher(pIntf)),
		m_flatCnt(0) {}
	~Impl() {}
// - см. примечание к Attribute::Impl::~Impl
	RI parse();
	bool dimensionWithinLimits();
	// - Сравнивает значение атрибута DIMENSION с ограничениями в ELEMENT-LIMIT
	//	 (функция должна вызываться не ранее, чем значение атрибута
	//	  ELEMENT-LIMIT будет прочитано или установлено)
	void afterRestoreDimension();	// - вычисляет m_flatCnt
	void notifyChanged(Item *i) {
		const Attribute::Impl *pai = dynamic_cast<const Attribute::Impl *>(i);
		if (!pai) return;

//		handleChanged(this, tReprCode(),  pai) ||
//		handleChanged(this, tUnits(),	  pai) ||
//		handleChanged(this, tDimension(), pai) ||
//		handleChanged(this, tElemLimit(), pai);

		bool chdCodeOrDim = handleChanged(this, tReprCode(),  pai) ||
							handleChanged(this, tDimension(), pai);
		if (chdCodeOrDim)
			setNullCurrentValue();
		else
			handleChanged(this, tUnits(),	  pai) ||
			handleChanged(this, tElemLimit(), pai);
	}
// Data -----------------------------------------------------------------------
	Representation::Code m_rc;
	Units m_u;
	vector<uint32> mv_dim;
		vector<uint32> mv_ellim;
	SingleValue m_val;
	ValueInterface m_valIntf;
	size_t m_flatCnt;	// - только для оптимизации
	// - NB: Поле m_flatCnt избыточно, т.к. определяется полем m_dim
}; // Channel::Impl

/*static*/ const AttributeMap *Channel::Impl::pAttributeMap() {
	static AttributeMap am;
	using namespace Representation;
	am.tagged = true;
	am.add("REPRESENTATION-CODE", 1, USHORT);
	am.add("UNITS",				  1, UNITS);
	am.add("DIMENSION",				 UVARI);
	am.add("ELEMENT-LIMIT",			 UVARI);
	// - NB: Порядок tagged-определений используется в тегах TagXXX
	am.tagged = false;
	am.add("LONG-NAME",	 1, OBNAME, ASCII);
	am.add("PROPERTIES",	IDENT		 );
	am.add("AXIS",			OBNAME		 );
	am.add("SOURCE",	 1, OBJREF		 );
	return &am;
}

//template <class T>
//RI Channel::Impl::getCurrentValue(T &value, size_t flatIndex) const
//	{ return m_val.get(value, flatIndex); }

//template <class T>
//RI Channel::Impl::getCurrentValue(vector<T> &value) const
//	{ return m_val.getAll(value); }

template <class T>
RI Channel::Impl::getCurrentValue(T *p,
										size_t flatIndex, size_t count) const {
	return m_val.get(p, flatIndex, count);
}

//template <class T>
//RI Channel::Impl::setCurrentValue(const T &value)
//	{ return m_val.set(value, m_rc); }
template <class T>
RI Channel::Impl::setCurrentValue(const T *p)	{
	return m_val.set(p, m_flatCnt, m_rc);
}

void Channel::Impl::read(Input &input_file) {
	Object::Impl::read(input_file);
	RI ri = parse();
//	if (ri.retCode() == RI::BadChan) throw ri;
		if (ri.critical()) throw ri;
		if (!ri.ok()) input_file.addIssue(ri);
}

//RI Channel::Impl::parse() {
//	return restoreAttribute(this, tReprCode()).
//		   upTo(restoreAttribute(this, tUnits())).
//		   upTo(restoreAttribute(this, tDimension())).
//		   upTo(restoreAttribute(this, tElemLimit())).
//		   upTo(checkDimension());
//}
RI Channel::Impl::parse() {
	bool ok = restoreAttribute(this, tReprCode()) &&
			  restoreAttribute(this, tDimension());
	if (!ok) return RI(RI::BadChan, 1).toCritical();
	ok = restoreAttribute(this, tElemLimit()) &&
		 dimensionWithinLimits() &&
		 restoreAttribute(this, tUnits());
	if (ok) return RI();
	else return RI(RI::BadChan, 2).toCritical();
}

bool Channel::Impl::frameable() const {
	return hasAttribute(tReprCode()) &&
		   hasAttribute(tDimension()) && hasAttribute(tElemLimit());
}

bool Channel::Impl::dimensionWithinLimits() {
	if (mv_dim.size() > mv_ellim.size()) return false;
	for (uint n = 0; n < mv_dim.size(); ++n)
		if (mv_dim[n] > mv_ellim[n]) return false;
	return true;
}

void Channel::Impl::afterRestoreDimension() {
	m_flatCnt = 1;
	for (uint ndim = 0; ndim < mv_dim.size(); ++ndim)
		m_flatCnt *= mv_dim[ndim];
}

void Channel::Impl::readCurrentValue(Input &input_file)	{
	m_val.read(input_file, m_rc, m_flatCnt);
}

void Channel::Impl::writeCurrentValue(Output &out) const {
	m_val.write(out);
}


Channel::Channel(ObjectParent *parent, const ObjectName &name) :
	Object(parent, name),
	pim(new Impl(this, parent, name))
{
	delete Object::pim;
	Object::pim = pim;
}

Channel::~Channel() {
	delete pim;
	Object::pim = NULL;
}

Representation::Code Channel::representationCode() const
	{ return pim->m_rc; }

Units &Channel::units() const { return pim->m_u; }

const vector<uint32> &Channel::dimension() const	{ return pim->mv_dim; }

size_t Channel::flatCount() const { return pim->m_flatCnt; }

RI Channel::setRepresentationCode(Representation::Code code) {
	if (!Representation::valid(code)) return RI(RI::BadRepr).toCritical();
	pim->setAttribute(pim, Impl::tReprCode(), code);
	return RI();
}

RI Channel::setUnits(const Units &units) {
	if (!units.valid()) return RI(RI::BadUnits, 2).toCritical();
	pim->setAttribute(pim, Impl::tUnits(), units);
	return RI();
}

RI Channel::setDimension (uint32 dim) {
	vector<uint32> dims(1);
	dims[0] = dim;
	return setDimension(dims);
}

RI Channel::setDimension (const vector<uint32> &dims) {
	for (uint n = 0; n < dims.size(); ++n)
		if (dims[n] < 1 || dims[n] > Representation::maxUVARI)
			return RI(RI::BadDim).toCritical();
	pim->setAttribute(pim, Impl::tElemLimit(), dims);
	pim->setAttribute(pim, Impl::tDimension(), dims);
	return RI();
}

const ValueInterface &Channel::ccurrentValue() const
	{ return pim->m_valIntf; }

ValueInterface &Channel::currentValue()
	{ return pim->m_valIntf; }


typedef const vector<const Channel *> ConstChannels;
typedef const vector<Channel *> Channels;


//class Frame : public Value {
//public:
//	uint32 number() const { return m_num; }
//	void fillChannelValues(Channels &chans) const;
//	void read(Input &in, const ConstChannels &chans);
//private:
//	uint32 m_num;
//};
class Frame {
public:
	Frame() : m_num(0), m_pIflr(NULL), m_pSlots(0) {}
	~Frame() {
		delete m_pIflr;
	}
	void clear() {
		clearSlots();
		location.clear();
		m_num = 0;
		m_pSlots = 0;
	}
	void clearSlots() {
		delete m_pIflr;
		m_pIflr = NULL;
	}
// Функции чтения
	uint32 number() const { return m_num; }
	bool slotsEmpty() { return m_pIflr == NULL; }
	RI fillChannelValues(Channels &chans) const;
	RI assign(std::istream *isIFLRBody);
	void assignSlots(std::istream *isIFLRBody);
// Функции записи
	void increaseNumber() { ++m_num; }
	// Поле, заполняемое и используемое только из LogicalFile::Impl
	void write(Output &out, ConstChannels &chans);
	LogicalRecordLocation location;
private:
	uint32 m_num;
// Поля только для режима чтения
	std::istream *m_pIflr;	// Логическая запись (IFLR), содержащая фрейм
	std::streampos m_pSlots;	// Позиция данных каналов в логической записи
};
// - Время чтения RST.dlis:
//	для старого варианта Frame (хранение Value) - 0.29 с
//	для нового варианта Frame (хранение stream) - 0.27 с

RI Frame::assign(std::istream *isIFLRBody) 
{
	m_pIflr = isIFLRBody;
    
	Input    input_file(m_pIflr);
	SingleValue::read(m_num, input_file, Representation::UVARI);
// - NB: Здесь возможна единственная ошибка: DataTrunc, генерирующая исключение
	m_pSlots = input_file.getPos();

	if (m_num == 0) 
        return RI(RI::BadFrameNum, 1).toCritical();
	else 
        return RI();
}

void Frame::assignSlots(std::istream *isIFLRBody) 
{
	assert(m_pIflr == NULL);
	m_pIflr = isIFLRBody;
}

RI Frame::fillChannelValues(Channels &chans) const {
	ErrorLogImpl err;
	m_pIflr->seekg(m_pSlots);
	Input input_file(m_pIflr, &err);
	for (unsigned nch = 0; nch < chans.size(); ++nch)
		chans[nch]->pim->readCurrentValue(input_file);
	if (!input_file.atEnd()) err.add(RI::ShortFrame);
	return err.getRetInfo();
}

void Frame::write(Output &out, ConstChannels &chans) {
	SingleValue::write(out, m_num, Representation::UVARI);
	for (unsigned nch = 0; nch < chans.size(); ++nch)
		chans[nch]->pim->writeCurrentValue(out);
}

//// ============================================================================

//class OutputFrame {
//public:
//	OutputFrame() { clear(); }
//	void clear() { m_num = 0; }
//	void increaseNumber() { ++m_num; }
//	void write(std::ostream *osIFLRBody, ConstChannels &chans);
//private:
//	uint32 m_num;
//};

//void OutputFrame::write(std::ostream *osIFLRBody, ConstChannels &chans) {
//	Output out(osIFLRBody);
//	for (unsigned nch = 0; nch < chans.size(); ++nch)
//		chans[nch]->pim->writeCurrentValue(out);
//}

// ============================================================================

class FrameType::Impl : public Object::Impl {
public:
	static const AttributeMap *pAttributeMap();
	bool allChannelsLinked() const {
		return mvr_chans.size() == mv_chnms.size();
	}
	RI linkChannels(LogicalFile::ObjectIt beginChannel,
					LogicalFile::ObjectIt endChannel);
// Функции чтения
	ConstChannels &cchannels() const { return (ConstChannels &)mvr_chans; }
	uint32 frameCount() const { return mvp_frames.size(); }
	bool frameLoaded(uint32 index) { return !mvp_frames[index]->slotsEmpty();}
	const LogicalRecordLocation &frameLocation(uint32 index) const {
		return mvp_frames[index]->location;
	}
	RI fillChannelValues(uint32 frameIndex) const;
	void read(Input &input_file);
//	RI makeNextFrame(std::istream *isIFLRBody,
//					 const LogicalRecordLocation *loc = NULL);
//	// - Создает экземпляр Frame для очередного фрейма и передает ему
//	//	 в *isIFLRBody тело логической записи, содержащей фрейм.
//	/*   *loc задает положение IFLR-записи с фреймом во входном потоке (чтобы
//		 фрейм мог быть загружен позже).
//		 В созданный экземпляр Frame:
//		 - если loc==NULL, сохраняется IFLR-запись с фреймом,
//		 - если loc!=NULL, записывается loc и положение данных каналов (Slots)
//		   в IFLR-записи */
	RI makeNextFrame(std::istream *isIFLRBody,
					 const LogicalRecordLocation &loc);
	// - Создает экземпляр Frame для очередного фрейма.
	/* - Параметр isIFLRBody - указатель на поток, содержащий тело логической
		 записи с фреймом;
		 при loadFrames()==false экземпляр Frame становится владельцем потока,
		 при loadFrames()==true поток уничтожается.
		 Параметр loc - положение IFLR-записи с фреймом во входном потоке
		 (чтобы фрейм мог быть загружен позже);
		 при loadFrames()==true) не используется */
	void assignSlots(uint32 frameIndex, std::istream *isIFLRBody);
	// - Передает логическую запись *isIFLRBody с фреймом экземпляру Frame
	//	 с индексом frameIndex (только для режима loadFrames()==false)
//	void clearSlots(uint32 frameIndex);
//	// - Очищает данные каналов фрейма
// Функции записи
	Channels &channels() const { return (Channels &)mvr_chans; }
	void writeNextOutFrame(Output &out);
private:
	friend class FrameType;
	typedef AttributeTag<Impl, vector<ObjectName> > TagChannels;
	static const TagChannels &tChannels() {
		static TagChannels t(0, &Impl::mv_chnms);
		return t;
	}
// ----------------------------------------------------------------------------
	Impl(ObjectParent *parent, const ObjectName &name) :
		Object::Impl(parent, name),
		m_frix(-1) {}
	~Impl() {
		freeMem();
	}
	void clear() {
		freeMem();
		mvr_chans.clear();
		mv_chnms.clear();
		m_frix = -1;
	}
	void freeMem() {
		clearPtrContainer(mvp_frames);
	}
// Функции чтения
	RI parse();
	bool loadFrames() const 
    { 
        return m_rparent->loadFramesToMemory(); 
    }

// Функции записи
	void notifyChanged(Item *i) {
		const Attribute::Impl *pa = dynamic_cast<const Attribute::Impl *>(i);
		if (!pa) return;
		bool chansChanged = handleChanged(this, tChannels(), pa);
		if (chansChanged) m_rparent->notifyChanged(this);
	}
// Data -----------------------------------------------------------------------
	vector<ObjectName> mv_chnms;
	vector<Channel *> mvr_chans;
	vector<Frame *> mvp_frames;
// Поля для режима чтения
	uint32 m_frix;
	// - Индекс текущего загруженного фрейма при loadFrames()==false
// Поля для режима записи
	Frame m_frout;
}; // class FrameType::Impl
//TODO: Полезно сделать для FRAME-объектов свой итератор
//		(в частности, для того, чтобы не приходилось применять dynamic_cast)
//TODO: В интерфейсе FrameType лучше возвращать ссылки на каналы
//		как ссылку на константный вектор

/*static*/ const AttributeMap *FrameType::Impl::pAttributeMap() {
	using namespace Representation;
	static AttributeMap am;
	am.tagged = true;
	am.add("CHANNELS", OBNAME);
	am.tagged = false;
	am.add("DESCRIPTION", 1, ASCII );
	am.add("INDEX-TYPE",  1, IDENT );
	am.add("DIRECTION",	  1, IDENT );
	am.add("SPACING",	  1		   );
	am.add("ENCRYPTED",	  1, USHORT);
	am.add("INDEX-MIN",   1		   );
	am.add("INDEX-MAX",	  1		   );
	return &am;
}

void FrameType::Impl::read(Input &input_file) {
	clear();
	Object::Impl::read(input_file);
	RI ri = parse();
	if (ri.retCode() == RI::BadFrameObj) throw ri;
}
// - NB: Код практически идентичен коду Channel::Impl::read

//RI FrameType::Impl::parse() {
//	const Attribute *pa;
//	bool ok;
//	pa = pcAttribute("CHANNELS");
//	ok = pa != NULL && pa->representationCode() == Representation::OBNAME &&
////		 pa->getValues(mv_chnms).ok();
//		 pa->cvalue().get(mv_chnms).ok();
//	if (!ok) return RI(RI::BadFrameObj);
//	return RI();
//}
RI FrameType::Impl::parse() {
	if (!restoreAttribute(this, tChannels()))
		return RI(RI::BadFrameObj).toCritical();
	return RI();
}

RI FrameType::Impl::linkChannels(LogicalFile::ObjectIt beginChannel,
								   LogicalFile::ObjectIt endChannel) {
	RI ri;
		mvr_chans.clear();
	for (unsigned n = 0; n < mv_chnms.size(); ++n) {
		LogicalFile::ObjectIt ito = findByKey(beginChannel, endChannel,
											  Object::nameOf, mv_chnms[n]);
//		if (ito == endChannel) return RI(RI::NoChan);
			if (ito == endChannel) continue;
/* - NB: Хотя отсутствие канала является критической ошибкой в режиме чтения
		 (т.к. в этом случае невозможно читать данные из фреймов), в режиме
		 это может быть промежуточным состоянием (проверка наличия всех каналов
		 будет выполняться непосредственно перед записью первого фрейма путем
		 простого сравнения числа элементов в mv_chnms и mvr_chans) */
		Channel *po = dynamic_cast<Channel *>(*ito);
		if (!po->pim->frameable()) ri.upTo(RI(RI::BadChan, 3));
		/* - NB: фактически только на случай отсутствия у канала атрибута
				 ELEMENT_LIMIT при чтении (т.к. каналы с другими отсутствующими
				 нужными атрибутами были удалены ранее) */
		mvr_chans.push_back(po);
	}
	return ri;
}

//RI FrameType::Impl::makeNextFrame(std::istream *isIFLRBody) {
//	Frame *pf = new Frame;
//	RI ri = pf->assign(isIFLRBody);
//	if (pf->number() != mvp_frames.size() + 1) ri.upTo(RI(RI::BadFrameNum, 2));
//	mvp_frames.push_back(pf);
//	return ri;
//}

//void FrameType::Impl::makeNextFrame(const LogicalRecordLocation &loc) {
//	Frame *pf = new Frame;
//	pf->location = loc;
//	mvp_frames.push_back(pf);
//}

//RI FrameType::Impl::makeNextFrame(std::istream *isIFLRBody,
//								  const LogicalRecordLocation *loc) {
//	Frame *pf = new Frame;
//	RI ri = pf->assign(isIFLRBody);
//	if (pf->number() != mvp_frames.size() + 1) ri.upTo(RI(RI::BadFrameNum, 2));
//	if (loc) {
//		pf->location = *loc;
//		pf->clearSlots();
//	}
//	mvp_frames.push_back(pf);
//	return ri;
//}

RI FrameType::Impl::makeNextFrame(std::istream *file_body, const LogicalRecordLocation &loc) 
{
	Frame *frame = new Frame;
	RI     ri    = frame->assign(file_body);

	if (frame->number() != mvp_frames.size() + 1) 
        ri.upTo(RI(RI::BadFrameNum, 2));

	frame->location = loc;

	if (!loadFrames()) 
        frame->clearSlots();

	mvp_frames.push_back(frame);
	return ri;
}

void FrameType::Impl::assignSlots(uint32 frameIndex, std::istream *isIFLRBody) 
{
	assert(!loadFrames());

	if (m_frix != -1) 
        mvp_frames[m_frix]->clearSlots();

	mvp_frames[frameIndex]->assignSlots(isIFLRBody);
	m_frix = frameIndex;
}

//void FrameType::Impl::clearSlots(uint32 frameIndex) {
//	mvp_frames[frameIndex]->clearSlots();
//}

void FrameType::Impl::writeNextOutFrame(Output &out) {
	m_frout.increaseNumber();
	m_frout.write(out, cchannels());
}

RI FrameType::Impl::fillChannelValues(uint32 frameIndex) const {
	if (frameIndex >= mvp_frames.size()) return RI(RI::NoFrame, 1).toCritical();
	Frame *pf = mvp_frames[frameIndex];
	if (pf == NULL) return RI(RI::NoFrame, 2).toCritical();
	return pf->fillChannelValues(mvr_chans);
}

FrameType::FrameType(ObjectParent *parent, const ObjectName &name) :
	Object(parent, name),
	pim(new Impl(parent, name))
{
	delete Object::pim;
	Object::pim = pim;
}

FrameType::~FrameType() {
	delete pim;
	Object::pim = NULL;
}

const ConstChannels &FrameType::cchannels() const { return pim->cchannels(); }

const Channels &FrameType::channels() { return pim->channels(); }


//const uint32 FrameType::lastFrameNumber() const {
//	uint32 cnt = pim->mvp_frames.size();
//	return cnt == 0 ? 0 : pim->mvp_frames[cnt-1]->number();
//}
const uint32 FrameType::frameCount() const { return pim->mvp_frames.size(); }

RI FrameType::setChannels(const vector<ObjectName> &channelNames) {
	for (uint n = 0; n < channelNames.size(); ++n) {
		ObjectName name = channelNames[n];
		if (!name.valid()) throw RI(RI::BadObjName, 3).toCritical();
	}
	if (!allUnique(channelNames.begin(), channelNames.end()))
		return RI(RI::AmbigChans, 1).toCritical();
	pim->setAttribute(pim, Impl::tChannels(), channelNames);
	return RI();
}

RI FrameType::setChannels(const vector<Channel *> &channels) {
	/* TODO */
	return RI();
}


// ============================================================================

class SetParent /*: public Item*/ {
public:
	virtual void notifyChanged(Item *i) {}
	virtual bool loadFramesToMemory() const = 0;
};

class Set : public ObjectParent {
public:
	static bool Set::getEFLRType(EFLRType &eflrType, Object::Type type);
	static const AttributeMap *pAttributeMap(Object::Type type);
// ----------------------------------------------------------------------------
	Set(SetParent *parent, EFLRType e) :
		m_rparent(parent), m_eflrType(e), m_tpl(this) {}	// - для чтения
	Set(SetParent *parent, Object::Type type);				// - для записи
	~Set() {
		freeMem();
	}
	EFLRType eflrType() const { return m_eflrType; }
	SetParent *parent() { return m_rparent; }
	const string &objectTypeId() const { return m_c.type().str(); }
	Object::Type objectType() const {
		return Object::Impl::type(m_c.type().str());
	}
	const uint objectCount() const { return mvp_objs.size(); }
	const Object *pcObject(uint index) const { return mvp_objs[index]; }
	Object *pObject(uint index) { return mvp_objs[index]; }
	void read(Input &input_file);
//	const vector<Object *> &objects() const { return mvp_objs; }
	void eraseObject(uint index) { mvp_objs.erase(mvp_objs.begin() + index); }
// Функции для записи
	void write(Output &out) const;
	Attribute *addNewAttribute(const string &labelStr);
	bool addStdAttribute(const string &labelStr) {
		if (!pAttributeMap(objectType())->definition(labelStr))
			return false; // - labelStr не является меткой стандартного атрибута
		addNewAttribute(labelStr);
		return true;
	}	// - для запроса из дочернего Object
//	bool defineNewAttribute(const Ident &label) {
//		if (!m_tpl.addNewAttribute(label)) return false;
//		/**/
//		return true;
//	}
	Object *addNewObject(const ObjectName &name, bool createAttributes = true);
	/* - NB: Параметр createAttributes введен только для некоторой оптимизации
			 (и повышения надежности); его значение false используется только
			 в режиме чтения (см. Set::readNextObject), хотя в начале вызова
			 Object::read объект очищается */
private:
	class Component : public Dlis::Component {
	public:
		Component() { reset(); }
		Component(Object::Type t, const Ident &name = Ident(""));
		const Ident &name() const { return m_n; }
		const Ident &type() const { return m_t; }
	protected:
		RoleGroup roleGroup() { return rgSet; };
		void reset();
		void readCharacteristics(Input &input_file);
		void writeCharacteristics(Output &out) const;
	private:
		enum FormatBitPos {
			/* Reserved, must be 0 */
			bName = 3,
			bType = 4
		};
		// - см. примечание к классу Attribute::Impl::Component
		Ident m_n;
		Ident m_t;
	}; // class Component
// ----------------------------------------------------------------------------
	void clear() {
		freeMem();
		m_tpl.clear();
	}
	void freeMem() { clearPtrContainer(mvp_objs, &Object::deletePtr); }
// Функции для чтения
	bool loadFramesToMemory() const { return m_rparent->loadFramesToMemory(); }
	void readSetComponent(Input &input_file);
	void readTemplate(Input &input_file);
	void readNextObject(Input &input_file);
// Функции для записи
	void defineAttributes(const AttributeMap *map);

	// - NB: вызывается только из конструктора
	void notifyChanged(Item *i) 
    {
		FrameType::Impl *pfoi = dynamic_cast<FrameType::Impl *>(i);
		if (!pfoi) return;
		m_rparent->notifyChanged(pfoi);
	}

// Data
	SetParent       *m_rparent;
	EFLRType         m_eflrType;
	Component        m_c;
	Template         m_tpl;
//	const AttributeMap *mm_rAttrDefs;
	vector<Object *> mvp_objs;
}; // class Set

Set::Component::Component(Object::Type t, const Ident &name) {
// - NB: Параметр name на данный момент не используется
//		 (согласно стандарту, Set может не иметь имени)
	m_d.role = rSet;
	m_t = Object::typeId(t);
	m_d.fmt[bType] = true;
	m_n = name.str();
	m_d.fmt[bName] = !m_n.empty();
}

void Set::Component::reset() {
//	m_d.role = rSet;
	m_t.clear();
	m_n.clear();
}

void Set::Component::readCharacteristics(Input &input_file) 
{
	FormatBits f = format();

	for (int n = 0; n < bName; ++n)
		if (f[n]) throw RI(RI::BadCompDescr);

	if (f[bType]) 
        SingleValue::read(m_t, input_file, Representation::IDENT);

	if (f[bName]) 
        SingleValue::read(m_n, input_file, Representation::IDENT);

	if (m_t.empty()) 
        throw RI(RI::NoSetType);
}

void Set::Component::writeCharacteristics(Output &out) const {
	FormatBits f = format();
	if (f[bType]) SingleValue::write(out, m_t, Representation::IDENT);
	if (f[bName]) SingleValue::write(out, m_n, Representation::IDENT);
}


// class Set definitions
/*static*/ bool Set::getEFLRType(EFLRType &eflrType, Object::Type type) {
	Object::Type typeLo = Object::FILE_HEADER,
//				 typeHi = Object::UPDATE;
					 typeHi = Object::Type(Object::TypeOther - 1);
	if (type < typeLo || type > typeHi) return false;
	static EFLRType eflrTypes[Object::TypeCount];
	static bool ready = false;
	if (!ready) {
		for (int nType = typeLo; nType <= typeHi; ++nType) {
			EFLRType et;
			switch ((Object::Type)nType) {
			case Object::FILE_HEADER:
				et = FHLR; break;
			case Object::ORIGIN: case Object::WELL_REFERENCE_POINT:
				et = OLR; break;
			case Object::AXIS:
				et = AXIS; break;
			case Object::CHANNEL:
				et = CHANNL; break;
			case Object::FRAME: case Object::PATH:
				et = FRAME; break;
			case Object::COMMENT: case Object::MESSAGE:
				et = SCRIPT; break;
//			case Object::UPDATE:
//				et = UPDATE; break;
			default:
				et = STATIC;
			}
			eflrTypes[nType] = et;
		}
		ready = true;
	}
	eflrType = eflrTypes[type];
	return true;
}

/*static*/ const AttributeMap *Set::pAttributeMap(Object::Type type) {
	switch (type) {
	case Object::FILE_HEADER: return FileHeader::pAttributeMap();
	case Object::CHANNEL:	  return Channel::Impl::pAttributeMap();
	case Object::FRAME:		  return FrameType::Impl::pAttributeMap();
	default: return Object::Impl::pAttributeMap(type);
	}
}

Set::Set(SetParent *parent, Object::Type type) :
	m_rparent(parent), m_c(type), m_tpl(this) {
	if (!getEFLRType(m_eflrType, type))
			throw RI(RI::ProgErr, 9);
	m_tpl.initAttributes(pAttributeMap(type));
}

void Set::read(Input &input_file) 
{
	clear();

	readSetComponent(input_file);

	switch (m_c.role()) 
    {
	    case Component::rRdSet:  
            throw RI(RI::RdSet);

	    case Component::rRepSet: 
            throw RI(RI::RepSet);

	}

	// - Обработка ошибок будет выполнена вызывающей функцией
	readTemplate(input_file);

	if (input_file.atEnd())
		input_file.addIssue(RI::NonStdSet);       // - Set не содержит ни одного Object

	while (!input_file.atEnd()) 
        readNextObject(input_file);
}

void Set::readSetComponent(Input &input_file) 
{
	Component::Descriptor d;
	std::streampos        p = input_file.getPos();

	d.read(input_file);

	if (Component::roleGroupOf(d.role) != Component::rgSet)
		throw RI(RI::BadSet);

	input_file.setPos(p);
	m_c.read(input_file);
}


void Set::readTemplate(Input &input_file) 
{
	m_tpl.read(input_file);
}


void Set::readNextObject(Input &input_file) 
{
	Object *po = addNewObject(ObjectName(), false);

	if (m_eflrType == FHLR && objectType() != Object::FILE_HEADER)
		input_file.addIssue(RI::NonStdFHLR);
	// - NB: Проверка соответствия типа объекта типу EFLR пока
	//		 поддерживается только для FHLR
	// - NB: Объект *po в этом случае можно не создавать,
	//		 т.к. все равно он не будет доступен пользователю
	try 
    {
		po->pim->read(input_file);
	}
	catch(RI ri) 
    {
		delete po;

		mvp_objs.pop_back();
		if (ri.retCode() == RI::BadChan || ri.retCode() == RI::BadFrameObj)
			input_file.addIssue(ri);
		else
			input_file.addIssue(RI::BadObj);
	}
	catch(...) 
    {
		delete po;
		mvp_objs.pop_back();
		throw;
	}
}

void Set::write(Output &out) const {
	m_c.write(out);
	m_tpl.write(out);
	for (uint n = 0; n < objectCount(); ++n)
		pcObject(n)->pim->write(out);
}

Attribute *Set::addNewAttribute(const string &labelStr) {
	Attribute *pa = m_tpl.addNewAttribute(labelStr);
	if (pa) {
		for (uint n = 0; n < objectCount(); ++n)
			pObject(n)->pim->completeAttributes();
	}
	return pa;
}

Object *Set::addNewObject(const ObjectName &name, bool createAttributes) 
{
	Object *po;

	switch (objectType()) 
    {
	case Object::FILE_HEADER:
		po = new FileHeader(this, name); break;

	case Object::ORIGIN:
		po = new Origin(this, name); break;

	case Object::CHANNEL:
		po = new Channel(this, name); break;

	case Object::FRAME:
		po = new FrameType(this, name); break;

	default:
		po = new Object(this, name);

	}
	mvp_objs.push_back(po);
	po->pim->initialize(&objectTypeId(), &m_tpl, createAttributes);
	return po;
}
// - NB: Проверка уникальности имени выполняется на уровне LogicalFile
//		 (см. LogicalFile::addNewObject)

//=============================================================================

class LogicalFile::ObjectIteratorData {
public:
	ObjectIteratorData(Object::Type type = Object::TypeAny,
					   int ixSet = -1, int ixObj = -1)
		: m_type(type), indexSet(ixSet), indexObject(ixObj) {}
//	ObjectIteratorData(const ObjectIteratorData &other) {
//		m_type = other.m_type;
//		indexSet = other.indexSet;
//		indexObject = other.indexObject;
//	}
// - NB: Конструктор копирования генерируется по умолчанию
	Object::Type type() const { return m_type; }
	bool samePosition(const ObjectIteratorData &other) const {
		return indexSet == other.indexSet && indexObject == other.indexObject;
	}
	int indexSet, indexObject;
	// - Текущий индекс экземпляра Set в контейнере m_sets
	//	 и текущий индекс экземпляра Object в контейнере m_objs текущего Set
	//	 (поля indexXXX изменяются в функции LogicalFile::iterateObject)
// - NB: Использовать тип uint для полей, с которыми постоянно выполняются
//		 операции сложения/вычитания (здесь это поля indexXXX), рискованно
private:
	Object::Type m_type;
};

class LogicalFileParent /*: public Item*/ {
public:
	virtual bool loadFramesToMemory() const { return false; }
};

class LogicalFile::Impl : public SetParent {
public:
	static LogicalFile *logicalFileOf(const Object *ob);
	const string &id() const { return m_h.id(); }
	uint32 seqNum() const { return m_h.seqNum(); }
	const uint setCount() const { return mvp_sets.size(); }
	const Set *pcSet(uint index) const { return mvp_sets[index]; }
	Set *pSet(uint index) { return mvp_sets[index]; }
	uint indexOfSet(Object::Type type, uint indexFrom) const;
	uint indexOfSetBwd(Object::Type type, uint indexFrom) const;
//	int indexOfSet(Object::Type type, int indexFrom,
//				   bool backwards = false) const;
// - объединенный вариант (пока  не работает)
// Функции для чтения
	void read(Input &inDlis, bool &wasLast);
	// - Чтение логического файла из входного DLIS-потока inDlis
	/* - wasLast - признак того, был ли прочитанный логический файл последним */
	/* - Для ускорения чтения из файла используется промежуточный буфер на одну
		 видимую запись (при чтении из потока в памяти эта буферизация является
		 избыточной, но не должна значительно замедлять чтение) */
	void loadFrame(Input &inDlis,
				   const FrameType *frameType, uint32 frameIndex);
/* - NB: Возможно, логичнее было бы сделать функции loadIFLR и readIFLR
		 статическими, т.к. они используют только поля m_vr (буфер на одну
		 видимую запись) и m_vrpos (положение буфера), которые по сути должны
		 быть общими для всех логических файлов.
		 Тогда не понадобилась бы функция logicalFileOf, но m_vr и m_vrpos
		 пришлось бы передавать через параметры функций */
//		void ShowStat(std::ostream &os) {
//			Stat &s = m_stat;
//			os << "  Record Statistics" << "\n";
//			os << "    Visual Records: " << s.vrCnt << "\n";
//			os << "    Logical Records: " << s.eflrCnt + s.iflrCnt <<
//				  " (" << s.eflrCnt << " EFLRs + " << s.iflrCnt << " IFLRs)\n";
//			os << "    Logical Record Segments: " << s.lrsCnt << "\n";
//		}
// Функции для записи
	void notifyChanged(Item *i) {
		FrameType::Impl *pfti = dynamic_cast<FrameType::Impl *>(i);
		if (!pfti) return;
		pfti->linkChannels(pin->beginObject(Object::CHANNEL),
						   pin->endObject(Object::CHANNEL));
	}
	void writeMetadata(Output &outDlis);
	RI writeFrame(Output &outDlis, FrameType *frameType);
	void finishWrite(Output &outDlis) {
		if (!m_vr.outEmpty()) m_vr.write(outDlis);
		m_vr.clear();
	}
private:
	friend class LogicalFile;
	class Header {
	public:
		Header() : m_rfh(NULL), m_sn(0) {}
		uint32 seqNum() const { return m_sn; }
		const string &id() const { return m_id; }
		void clear() { m_rfh = NULL; m_sn = 0; m_id = ""; }
		void link(FileHeader *pfh) { m_rfh = pfh; }	// - для чтения
		void link(FileHeader *pfh, uint32 seqNum) {	// - для записи
			m_rfh = pfh;
			m_rfh->setSeqNum(seqNum);
			setId(m_id);
			// - только для того, чтобы, согласно стандарту, установить флаг
			//	 Value у Атрибута 'ID' (хотя строка m_id еще пустая)
		}
	// Функции для чтения
		RI parse() {
				if (!m_rfh) return RI(RI::ProgErr, 14).toCritical();
			return m_rfh->parse(m_sn, m_id);
		}
	// Функции для записи
		void setId(const string &id) {
			m_rfh->setId(id);
			RI ri = m_rfh->parse(m_sn, m_id);
				if (!ri.ok()) throw RI(RI::ProgErr, 10).toCritical();
		}
	private:
		FileHeader *m_rfh;	// Ссылка на объект в FHLR
		uint32 m_sn;
		string m_id;
	}; // class Header
// ----------------------------------------------------------------------------
	Impl(LogicalFile *pIntf, LogicalFileParent *parent);
	// - для чтения
	Impl(LogicalFile *pIntf, LogicalFileParent *parent,
		 uint32 seqNum, const ObjectName &defOri);
	// - для записи
	~Impl() {
		freeMem();
	}
	void freeMem() {
		clearPtrContainer(mvp_sets);
	}
	void clear();
	const FileHeader *pcFileHeader() const;
	FileHeader *pFileHeader() {
		return const_cast<FileHeader *>(pcFileHeader());
	}
	bool contains(const FrameType *frameType) const;
//	FrameType *pFrameType(const ObjectName &obn) {
//		ObjectIt iteFr = pin->endObject(Object::FRAME);
//		ObjectIt ito = findByKey(pin->beginObject(Object::FRAME), iteFr,
//								 Object::nameOf, obn);
///* - NB: Поиск можно оптимизировать, запоминая последний найденный тип фрейма
//		 (Frame Object), но это имеет смысл, если разных типов много */
//		if (ito != iteFr) return dynamic_cast<FrameType *>(*ito);
//		else return NULL;
//	} // - пока нет необходимости
// Функции для чтения
	bool loadFramesToMemory() const { return m_rparent->loadFramesToMemory(); }
	void readLogicalRecordBody(LogicalRecord &lr,
							   const LogicalRecordLocation &lrloc,
							   ErrorLogImpl *err);
	void makeNextSet(Input &inEFLRBody, EFLRType e);
	// - Читает содержимое EFLR и создает новый элемент Set
	void makeNextFrame(std::istream *isIFLRBody,
					   const LogicalRecordLocation &lrloc, ErrorLogImpl *err);
	// - Создает новый экземпляр Frame и передает ему (делает его владельцем)
	//	 поток *isIFLRBody, содержащий очередной фрейм;
	/* - lrloc - положение фрейма во входном DLIS-потоке, передаваемое
		 на хранение и далее используемое только из LogicalFile::Impl */
	void addSet(Set *ps);
	void eraseObject(LogicalFile::ObjectIt it);
	RI parseObjects();
	void readIFLR(LogicalRecord &iflr, Input &inDlis,
				  const LogicalRecordLocation &loc);
	// - Чтение IFLR-записи по ее положению loc во входном DLIS-потоке
// Функции для записи
	Set *addNewSet(Object::Type type);
	Set &set(Object::Type type) {
		uint i = indexOfSet(type, 0);
		return i == -1 ? *addNewSet(type) : *mvp_sets[i];
	}
	// - Возвращает первый Set типа type, при отсутствии создавая его
	void writeLogicalRecordBody(Output &outDlis, LogicalRecord &lr);
// Data -----------------------------------------------------------------------
	LogicalFile *const pin;
	LogicalFileParent *m_rparent;
	Header m_h;
	byte m_ori;
	vector <Set *> mvp_sets;
	VisibleRecord m_vr;
	// - Текущая видимая запись.
	/* - Используется:
		 - в режиме записи,
		 - в режиме чтения при loadFramesToMemory()==false */
// Поля для режима чтения
	std::streampos m_vrpos;	// Позиция (прочитанной) видимой записи m_vr
	bool m_crypt;
	bool m_parsed;	// - устанавливается в parseObjects
		struct Stat {
			Stat() { clear(); }
			void clear() { vrCnt = 0; lrsCnt = 0; eflrCnt = 0; iflrCnt = 0; }
			long vrCnt, lrsCnt, eflrCnt, iflrCnt;
		} m_stat;
}; // class LogicalFile::Impl

/*static*/ LogicalFile *LogicalFile::Impl::logicalFileOf(const Object *ob) {
	Set *ps = dynamic_cast<Set *>(ob->pim->parent());
	LogicalFile::Impl *plfi = dynamic_cast<LogicalFile::Impl *>(ps->parent());
	return plfi->pin;
}

LogicalFile::Impl::Impl(LogicalFile *pIntf, LogicalFileParent *parent)
	: pin(pIntf), m_rparent(parent), m_vrpos(-1)
{
	clear();
}

LogicalFile::Impl::Impl(LogicalFile *pIntf, LogicalFileParent *parent,
						uint32 seqNum, const ObjectName &defOri) :
	pin(pIntf), m_rparent(parent), m_ori(defOri.originId()), m_crypt(false)
{
	Object *pfh = addNewSet(Object::FILE_HEADER)->
				  addNewObject(ObjectName("x", m_ori));
	m_h.link(dynamic_cast<FileHeader *>(pfh), seqNum);
	addNewSet(Object::ORIGIN)->addNewObject(defOri);
	m_vr.newOutRecord();
}

uint LogicalFile::Impl::indexOfSet(Object::Type type, uint indexFrom) const {
	for (uint i = indexFrom; i < setCount(); ++i ) {
		if (type == Object::TypeAny || pcSet(i)->objectType() == type)
			return i;
	}
	return -1;
}
uint LogicalFile::Impl::indexOfSetBwd(Object::Type type, uint indexFrom) const {
	for (int i = indexFrom; i >= 0; --i ) {
		if (type == Object::TypeAny || pcSet(i)->objectType() == type)
			return i;
	}
	return -1;
}

//	int LogicalFile::Impl::indexOfSet(Object::Type type, int indexFrom,
//				   bool backwards = false) const {
//		int i = indexFrom;
//		while (!backwards ? i < setCount() : i >= 0) {
//			if (type == Object::TypeAny || pcSet(i)->objectType() == type)
//				return i;
//			i += !backwards ? +1 : -1;
//		}
//		return -1;
//	}
// - NB: Использование объединенного варианта приводит к ошибке;
//		 причину пока установить не удалось

void LogicalFile::Impl::clear() {
	m_h.clear();
	m_ori = 0;
	freeMem();
	m_crypt = false;
	m_parsed = false;
		m_stat.clear();
}

const FileHeader *LogicalFile::Impl::pcFileHeader() const {
	if (setCount() == 0) return NULL;
	const Set *ps = pcSet(0);
	if (ps->objectCount() == 0) return NULL;
	const FileHeader *pfh =
			dynamic_cast<const FileHeader *>(ps->pcObject(0));
		if (!pfh) return NULL;	// - program error
	return pfh;
}

void LogicalFile::Impl::read(Input &file_DLIS, bool &was_last) 
{
	clear();

	bool                  log_file_start = true;
	VisibleRecord         visible_rcd;              // Текущая видимая запись
	LogicalRecord         logical_rcd;              // Текущая логическая запись
	LogicalRecordLocation logical_rcd_location;

	// - Положение текущей логической записи
	/*
     - (нужно только при loadFramesToMemory()==false, но для простоты
	вычисляется всегда 
    */

	bool first_segment  = true;
	bool log_rcd_ended  = false;

	while (true) 
    {
	    // Читаем очередную видимую запись
		std::streampos vsbl_rcd_start = file_DLIS.getPos();

		visible_rcd.read(file_DLIS);

		Input   input_visible_rcd(visible_rcd.pData(), file_DLIS.pErrorLog() /*, -1*/);

		if (log_rcd_ended)
        {
		    // - Закончилась ли предыдущая видимая запись последним сегментом
		    //	 логической записи?
		    //   Проверяем, не начинает ли текущая видимая запись новый логический
		    //	 файл
			logical_rcd.newRecord(false);

			bool    log_rcd_ended;

			logical_rcd.readSegment(input_visible_rcd, log_rcd_ended);
			// - Считываем заголовок первого сегмента текущей видимой записи
			//	 для проверки, не начинает ли она новый логический файл
			// - NB: признак lrEnded здесь не используется
			if (logical_rcd.isFileHeader())
            {
				file_DLIS.setPos(vsbl_rcd_start);
				// - Восстанавливаем позицию потока на начале текущей видимой
				//	 записи (т.е. начале нового логического файла)
                // - NB: Прочитанные данные первой видимой записи нового логического файла
                //		 далее не используются (будут прочитаны заново);
                //		 это некритично, т.к. число логических файлов обычно мало
				break;
			}
			else 
            {
				input_visible_rcd.setPos(0);
				// - Восстанавливаем позицию входного потока видимой записи
				//	 на начале первого сегмента
				logical_rcd.newRecord();
			}
		} // if (lrEnded)

		while (input_visible_rcd.getPos() < visible_rcd.dataLength())
        {
		    // - Текущая видимая запись не закончилась?
		    //   Читаем очередной сегмент
			if (first_segment)
            {
				logical_rcd_location.visRecPos = vsbl_rcd_start;
				logical_rcd_location.ofs       = input_visible_rcd.getPos();
				first_segment                  = false;
			}

			logical_rcd.readSegment(input_visible_rcd, log_rcd_ended);
			++m_stat.lrsCnt;

			if (log_rcd_ended)
            {
                // Прочитан ли последний сегмент логической записи
			    // Обрабатываем накопленное тело логической записи
				if (log_file_start != logical_rcd.isFileHeader())
                    throw RI(RI::BadFHLRPos);

                // - Строго говоря, эта проверка имеет смысл только для первого
                //	 логического файла, для остальных же - это контроль
                //	 правильности программы
                // - NB: Проверку правильного положения Defining Origin лучше выполнить позже

				if (logical_rcd.encrypted()) 
                    m_crypt = true;
				else 
                    readLogicalRecordBody(logical_rcd, logical_rcd_location, file_DLIS.pErrorLog());

				log_file_start = false;
				logical_rcd.newRecord();
				first_segment  = true;
			} // if (lrEnded)

		} // while (inVR.getPos() < vr.dataLength())

	    ++m_stat.vrCnt;
		was_last = file_DLIS.atEnd();

		if (was_last) 
            break;

	}   // while (true)

} // LogicalFile::Impl::readBuffered

void LogicalFile::Impl::loadFrame(Input &inDlis, const FrameType *frameType,
								  uint32 frameIndex) {
	LogicalRecord iflr;
	readIFLR(iflr, inDlis, frameType->pim->frameLocation(frameIndex));
	frameType->pim->assignSlots(frameIndex, iflr.extractBody());
}

void LogicalFile::Impl::readIFLR(LogicalRecord &iflr, Input &inDlis,
								 const LogicalRecordLocation &loc) {
	bool firstVR = true;
	iflr.newRecord();
	while (true) {
	// Читаем очередную видимую запись, содержащую данную IFLR-запись
		if (firstVR) {
			if (loc.visRecPos != m_vrpos) {
				inDlis.setPos(loc.visRecPos);
				m_vrpos = loc.visRecPos;
				m_vr.read(inDlis);
			} else
				inDlis.setPos(loc.visRecPos + (std::streamoff)m_vr.length());
/* - NB: Судя по экспериментам, на выполнение функции позиционирования в потоке
		 ifstream::seekg() какое-то заметное время не требуется, но если это
		 не так, вызов inDlis.setPos() следует выполнять только
		 при необходимости, перед считыванием следующей видимой записи
		 (т.к. это считывание не понадобится, если вся логическая запись
		 находится внутри одной видимой записи) */
		} else {
			m_vrpos = inDlis.getPos();
			m_vr.read(inDlis);
		}
		Input inVR(m_vr.pData(), inDlis.pErrorLog());
// - NB: Второй параметр (пока) не используется, хотя ошибки могут возникать
		if (firstVR) inVR.setPos(loc.ofs);
		while (/*firstVR || */inVR.getPos() < m_vr.dataLength()) {
		// - Текущая видимая запись не закончилась?
		// Читаем очередной сегмент
			bool lrEnded;
			iflr.readSegment(inVR, lrEnded);
			if (lrEnded) return;
		}
		firstVR = false;
	} // while (true)
} // LogicalFile::Impl::readIFLR

//void LogicalFile::Impl::writeLogicalRecordBody(Output &outDlis,
//											   LogicalRecord &lr) {
//	bool lrEnded = false;
//	while (!lrEnded) {
//		Output outVR(m_vr.pOutData());
//			d_nfr290("a");
//		uint segCnt = lr.writeSegments(outVR, m_vr.outSpaceLeft(), lrEnded);
//		// - Число записанных (или дописанных) в m_vr сегментов, для которых
//		//	 хватило места (segCnt==0 означает, что места недостаточно и нужно
//		//	 начинать новую видимую запись)
//			d_nfr290("b");
//		if (segCnt > 0) continue;
//			if (lrEnded || m_vr.outEmpty()) throw RI(RI::ProgErr, 11);
//	// Записываем заполненную видимую запись в выходной поток
//			d_nfr290("c");
//		m_vr.write(outDlis);
//	// Начинаем новую видимую запись
//			d_nfr290("d");
//		m_vr.newOutRecord();
//			d_nfr290("e");
//	}
//}

void LogicalFile::Impl::writeLogicalRecordBody(Output &outDlis,
											   LogicalRecord &lr) {
	Output outVR(m_vr.pOutData());
	bool lrEnded = false;
	while (!lrEnded) {
		uint segCnt = lr.writeSegments(outVR, m_vr.outSpaceLeft(), lrEnded);
		// - Число записанных (или дописанных) в m_vr сегментов, для которых
		//	 хватило места (segCnt==0 означает, что места недостаточно и нужно
		//	 начинать новую видимую запись)
		if (segCnt > 0) continue;
			if (lrEnded || m_vr.outEmpty()) throw RI(RI::ProgErr, 11);
	// Записываем заполненную видимую запись в выходной поток
		m_vr.write(outDlis);
	// Начинаем новую видимую запись
		outVR.close();
		// - Закрываем объект outVR, т.к. при вызове m_vr.newOutRecord() поток,
		//	 с которым связан outVR, уничтожается
		m_vr.newOutRecord();
		outVR.open(m_vr.pOutData());
	}
}

void LogicalFile::Impl::writeMetadata(Output &outDlis) {
	for (uint n = 0; n < setCount(); ++n) {
		const Set *ps = pcSet(n);
		LogicalRecord lr(ps->eflrType());
		Output lrBody(lr.pOutBody());
		ps->write(lrBody);
		writeLogicalRecordBody(outDlis, lr);
	}
}

bool LogicalFile::Impl::contains(const FrameType *frameType) const {
	ObjectConstIt iteFr = pin->cendObject();
	return find(pin->cbeginObject(Object::FRAME), iteFr, frameType) != iteFr;
}

RI LogicalFile::Impl::writeFrame(Output &outDlis, FrameType *frameType) {
	if (!contains(frameType)) return RI(RI::NoFrameType).toCritical();
	if (!frameType->pim->allChannelsLinked())
		return RI(RI::NotAllChans).toCritical();
	LogicalRecord lr(LogicalRecord::FDATA);
	Output lrBody(lr.pOutBody());
	SingleValue::write(lrBody, frameType->name(), Representation::OBNAME);
/* - NB: Используем здесь (и в других местах) SingleValue::write
		 (а не Value::Convertor::toRaw) из соображений "симметричности" коду
		 операций чтения (в данном случае функции makeNextFrame) */
	frameType->pim->writeNextOutFrame(lrBody);
	writeLogicalRecordBody(outDlis, lr);
	return RI();
}

void LogicalFile::Impl::readLogicalRecordBody(LogicalRecord &lr, const LogicalRecordLocation &lrloc, ErrorLogImpl *err) 
{
	LogicalRecord::Type log_rcd_type = lr.getType();

    static int count = 0;

	if (log_rcd_type.isEFLR)
    {
		++m_stat.eflrCnt;
		Input lrBody(&lr.body(), err, -1);

        count ++;

		makeNextSet(lrBody, log_rcd_type.e);

// - NB: Передать временный экземпляр Input (с вызовом конструктора
//		 непосредственно при передаче параметра) невозможно, т.к. требуется
//		 передача по ссылке без атрибута const
	} 
    else 
    {
		++m_stat.iflrCnt;
		if (!m_parsed) 
        {
			m_parsed = true;
			parseObjects();
		}

        /* - NB: Правильно ли устанавливать связь между FRAME- и CHANNEL-объектами
		(а также выполнять некоторые проверки) в parseObjects здесь?
		Но это зависит, например, от того, могут ли CHANNEL-объекты
		находиться после ссылающихся на них FRAME-объектов, а также от того,
		могут ли FRAME- и CHANNEL-объекты находиться после IFLR-записей */

		if (log_rcd_type.i == LogicalRecord::FDATA)
        {
			std::istream *pb = lr.extractBody();
			try 
            {
				makeNextFrame(pb, lrloc, err);
			}
			catch (...) 
            {
				delete pb; throw;
			}
		}
		else
			err->add(RI::NonFrameData);
	}
}

void LogicalFile::Impl::makeNextSet(Input &input_body_EFLR, EFLRType type) 
{
	Set *set = new Set(this, type);

	try 
    {
        // inBody.addIssue(RI::BadTpl);	// DEBUG
		set->read(input_body_EFLR);
	}
	catch(RI ri) 
    {
		delete set;
		switch (ri.retCode()) 
        {
            case RI::RepSet:                
                input_body_EFLR.addIssue(RI::RepSet); 
                return;

		    case RI::RdSet:  
                return;

		    default: 
                throw;
		}
	}
	catch(...) 
    {
		delete set; throw;
	}

	if (set->objectCount() == 0) 
    {
		delete set; 
        return;
	}

	addSet(set);
	if (mvp_sets.size() == 1) 
    {
		FileHeader *pfh = pFileHeader();
        // Анализируем File Header (в первом Set)
		if (!pfh) 
        {
			input_body_EFLR.addIssue(RI::BadFH);
            return;
		}

//		if (inEFLRBody.lastValidPos() != (std::streampos)120)
        /* - NB: Т.к. std::streampos является классом с несколькими полями (причем,
        как показывает отладочное окно, значение позиции в потоке описывается,
        похоже, комбинацией двух полей), приведение целого числа к типу
        std::streampos ненадежно */
		if ((size_t)(input_body_EFLR.lastValidPos()) != 120)
            input_body_EFLR.addIssue(RI::NonStdFH, 4);
			
		m_h.link(pfh);
		RI ri = m_h.parse();

		if (!ri.ok()) 
            input_body_EFLR.addIssue(ri);
	}
}

void LogicalFile::Impl::makeNextFrame(std::istream *isIFLRBody, const LogicalRecordLocation &lrloc, ErrorLogImpl *err) 
{
	Input        inBody(isIFLRBody, err, -1);
	ObjectName   obn;

	SingleValue::read(obn, inBody, Representation::OBNAME);
    
	if (obn.ident().empty()) 
    {
		inBody.addIssue(RI::BadFrame, 1); 
        return;
	}

	ObjectIt iteFr = pin->endObject(Object::FRAME);
	ObjectIt ito   = findByKey(pin->beginObject(Object::FRAME), iteFr, Object::nameOf, obn);
    /* - NB: Поиск можно оптимизировать, запоминая последний найденный тип фрейма
	(Frame Object), но это имеет смысл, если разных типов много */
	if (ito == iteFr) 
    {
		inBody.addIssue(RI::BadFrame, 2); 
        return;
	}

	inBody.close();
	// - т.к. поток isIFLRBody далее передается в FRAME-объект
	//	 (где может быть уничтожен)

	FrameType *pft = dynamic_cast<FrameType *>(*ito);
//	const LogicalRecordLocation *ploc = m_rparent->loadFramesToMemory() ?
//											NULL : &m_lrloc;
//	RI ri = pft->pim->makeNextFrame(isIFLRBody, ploc);

	RI ri = pft->pim->makeNextFrame(isIFLRBody, lrloc);

	if (!ri.ok()) 
        inBody.addIssue(ri);
}

void LogicalFile::Impl::addSet(Set *ps) {
	mvp_sets.push_back(ps);
}

Set *LogicalFile::Impl::addNewSet(Object::Type type) {
	addSet(new Set(this, type));
	return mvp_sets.back();
}

void LogicalFile::Impl::eraseObject(LogicalFile::ObjectIt it) {
	const LogicalFile::ObjectIteratorData *pd = it.data();
	mvp_sets[pd->indexSet]->eraseObject(pd->indexObject);
}

RI LogicalFile::Impl::parseObjects() 
{
	RI   ri;
	bool ok;

	ok = allUniqueKeys(pin->cbeginObject(Object::CHANNEL), pin->cendObject(), Object::nameOf);
	if (!ok) 
        return RI(RI::AmbigChans, 2).toCritical();

//	for (int ntype = 0; ntype < Object::typeCount; ++ntype) {
//		Object::Type ot = (Object::Type)ntype;
//		if (ot == Object::typeUndefined || ot == Object::TypeOther) continue;
//		ok = allUniqueKeys(pin->cbeginObject(ot), pin->cendObject(),
//						   Object::nameOf);
//		if (!ok) { ri = RI::AmbigObjs; break; }
//	}
// - NB: Не выполняет проверку уникальности объектов типов Object::TypeOther

	ok = allUniqueKeys(pin->cbeginObject(), pin->cendObject(), Object::referenceTo);
	// - NB: Использование Object::referenceTo позволяет простым образом
	//		 проверить уникальность имен объектов всех типов, в том числе
	//		 входящих в Object::TypeOther
	// - NB: Значительно замедляет выполнение (см. замеры ниже)!
	if (!ok) 
        ri = RI(RI::AmbigObjs);

/* Время выполнения функции Reader::Impl::read (release-сборка)
   на файле RST.dlis (39 МБайт)
   - без проверки уникальности: 0.27 с
   - с проверкой в цикле по типам (кроме TypeOther) через Object::nameOf: 0.27 с
   - с проверкой через Object::referenceTo: 0.55 с
	 = то же с отключением собственно операций получения ключей и их сравнения
	   в allUniqueKeys: 0.27 с
	   (вывод: собственно организация итератора ObjectConstIt не является
	   причиной замедления, просто число операций сравнения велико)
	 = то же с использованием в allUniqueKeys вызова findByKey: 0.41 с
   После изменения порядка сравнения в ObjectReference::operator==
   (сначала сравнение имен, потом типов)
   - с проверкой через Object::referenceTo: 0.58 с
*/
/* - NB: Для ускорения проверок на уникальность можно попробовать одно из
		 следующих решений:
		 1) использовать вложенную пару map-контейнеров: ключ внешнего
			контейнера - тип объекта, ключ внутреннего - его имя (при этом
			естественным образом появляется возможность еще одного варианта
			реализации итераторов ObjectXXXIt)
		 2) хранить экземпляр ObjectReference непосредственно в Object
			и возвращать его по ссылке (вместо ObjectReference можно
			использовать некий вспомогательный класс, хранящий ссылки на члены
			Object::Impl, содержащие тип и имя объекта) */

	ObjectIt itbCh = pin->beginObject(Object::CHANNEL),
			 iteCh = pin->endObject(Object::CHANNEL);

	ObjectIt itbFr = pin->beginObject(Object::FRAME),
			 it = pin->endObject(Object::FRAME);

	if (it != itbFr) 
    {
		--it;
		while (true) 
        {
			bool atbegin = it == itbFr;

			FrameType *pfo  = dynamic_cast<FrameType *>(*it);
			RI         riCh = pfo->pim->linkChannels(itbCh, iteCh);

//			if (riCh.retCode() == RI::NoChan) eraseObject(it);
            if (!pfo->pim->allChannelsLinked()) 
                eraseObject(it);
			// - Удаляем FRAME-объект, если не все его CHANNEL-объекты найдены
			else if (!atbegin) 
                --it;
// - NB: Декремент данного итератора за пределами контейнера приводит к ошибке:
//		 соответствует ли это требованиям к итераторам?
			ri.upTo(riCh);

			if (atbegin) 
                break;
		}
	}
// - NB: Надо правильнее организовать этот цикл или, еще лучше, ввести
//		 reverse-итератор
	return ri;
} // LogicalFile::Impl::parseObjects

LogicalFile::LogicalFile(LogicalFileParent *parent) :
	pim(new Impl(this, parent)) {}

LogicalFile::LogicalFile(LogicalFileParent *parent,
						 uint32 seqNum, const ObjectName &defOri) :
	pim(new Impl(this, parent, seqNum, defOri)) {}

LogicalFile::~LogicalFile() {
	delete pim;
}

uint32 LogicalFile::seqNum() const { return pim->m_h.seqNum(); }

const string &LogicalFile::id() const {	return pim->m_h.id(); }

uint32 LogicalFile::definingOriginId() const {
	const Object *po = cdefiningOrigin();
	return po ? po->name().originId() : -1;
}

bool LogicalFile::anyEncrypted() const { return pim->m_crypt; }

RI LogicalFile::setId(const string &id) {
	pim->m_h.setId(id);
	return pim->m_h.id().length() < id.length() ? RI(RI::FHIdTrunc) : RI();
}

RI LogicalFile::introduceAttribute(Object::Type type, const string &labelStr,
								   bool standardOnly) {
	try {
		if (!Object::Impl::typeIsCertain(type)) throw RI(RI::BadObjType);
		Ident id = Ident(labelStr);
		if (standardOnly && !Set::pAttributeMap(type)->definition(labelStr))
				throw RI(RI::NonStdAttr);
		if (!id.valid()) throw RI(RI::BadAttrLabel);
		if (!pim->set(type).addNewAttribute(labelStr)) return RI::DupAttr;
		else return RI();
	}
	catch(RI ri) {
		return ri.toCritical();
	}
}

Object *LogicalFile::addNewObject(Object::Type type, const ObjectName &name,
								  RI *ri) {
	Object *po;
	try {
		if (!Object::Impl::typeIsCertain(type)) throw RI(RI::BadObjType);
		if (type == Object::FILE_HEADER) throw RI(RI::NoUserFH);
		if (!name.valid()) throw RI(RI::BadObjName, 1);
		if (cobject(type, name)) throw RI(RI::DupObj);
		po = pim->set(type).addNewObject(name);
		if (po == NULL) throw RI(RI::AddObjErr);
	}
	catch(RI riGot) {
		if (ri) *ri = riGot.toCritical();
		return NULL;
	}
	return po;
}

Object *LogicalFile::addNewObject(Object::Type type, const string &nameIdStr,
								  uint32 originId, byte copyNum, RI *ri) {
	Object *po;
	try {
		if (originId == -1) originId = pim->m_ori;
//		po = addNewObject(type, ObjectName(nameIdStr, originId, copyNum));
		po = addNewObject(type, ObjectName(nameIdStr, originId, copyNum), ri);
	}
	catch(RI riGot) {
		if (ri) *ri = riGot.toCritical();
		return NULL;
	}
	return po;
}

const Origin *LogicalFile::cdefiningOrigin() const	{
	LogicalFile::ObjectConstIt itb = cbeginObject(Object::ORIGIN);
	LogicalFile::ObjectConstIt ite = cendObject();
	return (itb == ite) ? NULL : dynamic_cast<const Origin *>(*itb);
}
//TODO: Ввести функцию 'int objectCount(Object::Type)', чтобы такие функции,
//		как cdefiningOrigin, записывались короче

Origin *LogicalFile::definingOrigin()
	{ return const_cast<Origin *>(cdefiningOrigin()); }

LogicalFile::ObjectConstIt
LogicalFile::cbeginObject(Object::Type type) const {
	ObjectIteratorData d = ObjectIteratorData(type);
	iterateObject(d, true);	// "Продвигаемся" к первому объекту типа type
//		iterateObject(d);	// "Продвигаемся" к первому объекту типа type
	return ObjectConstIt(this, d);
}

LogicalFile::ObjectConstIt LogicalFile::cendObject() const {
	return ObjectConstIt(this, ObjectIteratorData(Object::TypeAny,
												  pim->mvp_sets.size(), 0));
}

LogicalFile::ObjectIt
LogicalFile::beginObject(Object::Type type) {
	ObjectIteratorData d = ObjectIteratorData(type);
	iterateObject(d, true);	// "Продвигаемся" к первому объекту типа type
//		iterateObject(d);	// "Продвигаемся" к первому объекту типа type
	return ObjectIt(this, d);
}

LogicalFile::ObjectIt LogicalFile::endObject(Object::Type type) {
	return ObjectIt(this, ObjectIteratorData(type, pim->mvp_sets.size(), 0));
}
/* - NB: Т.к. это bidirectional-итератор, возвращаемое значение функции
		 endObject тоже зависит от типа DLIS-объекта (в отличие от функции
		 cendObject), поэтому есть опасность зацикливания при случайном указании
		 разных типов при вызове beginObject и endObject в условии цикла */

const Object *LogicalFile::cobject(Object::Type type,
								   const ObjectName &name) const {
	ObjectConstIt ito = findByKey(cbeginObject(type), cendObject(),
								  Object::nameOf, name);
	if (ito == cendObject()) return NULL;
	else return *ito;
}

const Object *LogicalFile::cobject(Object::Type type, const string &nameIdStr,
								   uint32 originId, byte copyNum) const {
	if (originId == -1) originId = pim->m_ori;
	return cobject(type, ObjectName(nameIdStr, originId, copyNum));
}


LogicalFile::ObjectIteratorData
*LogicalFile::copy(const LogicalFile::ObjectIteratorData &d) const
	{ return new ObjectIteratorData(d); }

bool LogicalFile::same(const ObjectIteratorData &d1,
					   const ObjectIteratorData &d2) const
	{ return d1.samePosition(d2); }
// - NB: При сравнении итераторов учитываем только положение текущего,
//		 DLIS-объекта, но не его тип (ObjectIteratorData::type) - в частности,
//		 для унификации функции LogicalFile::cendObject

const Object *LogicalFile::cgetObject(const ObjectIteratorData &d) const
	{ return pim->pcSet(d.indexSet)->pcObject(d.indexObject); }

Object *LogicalFile::getObject(const ObjectIteratorData &d)
	{ return pim->pSet(d.indexSet)->pObject(d.indexObject); }

void LogicalFile::iterateObject(ObjectIteratorData &d, bool fwd /*backwards*/ ) const {
	if (fwd /*!backwards*/) {
		++d.indexObject;
		bool toNextSet = d.indexSet == -1 ||
				d.indexObject >= pim->pcSet(d.indexSet)->objectCount();
		if (toNextSet) {
//			d.indexSet = pim->indexOfSet(d.type(), d.indexSet + 1);
//			if (d.indexSet == -1) d.indexSet = pim->setCount();

			bool found, foundEmpty;
			// Находим следующий непустой Set нужного типа
			do {
				d.indexSet = pim->indexOfSet(d.type(), d.indexSet + 1);
				found = d.indexSet != -1;
				foundEmpty = found &&
							 pim->mvp_sets[d.indexSet]->objectCount() == 0;
			} while (foundEmpty);
			if (!found) d.indexSet = pim->setCount();

			d.indexObject = 0;
		}
	}
	else {
		--d.indexObject;
		bool toNextSet = d.indexObject == -1;
		if (toNextSet) {
			d.indexSet = pim->indexOfSetBwd(d.type(), d.indexSet - 1);
//				d.indexSet = pim->indexOfSet(d.type(), d.indexSet - 1, true);
			d.indexObject = pim->pcSet(d.indexSet)->objectCount() - 1;
		}
	}
}

// ============================================================================

class StorageUnitLabel {
public:
	StorageUnitLabel() { reset(); }
	unsigned getSeqNum() const {
		long n;
		bool ok = charsToInt(seqNum, sizeof seqNum, n) && n > 0;
		return (ok) ? n : 0;
	}
	bool isValidVersion1() const {
		string s(dlisVersion, sizeof dlisVersion);
		return s.length() == 5 && s.substr(0, 3) == "V1." &&
			   isdigit(s[3]) && isdigit(s[4]);
	}
	uint getMaxVisRecLen() const {
		long n;
		bool ok = charsToInt(maxVisRecLen, sizeof maxVisRecLen, n) &&
				  ((n == 0) || (20 <= n && n <= VisibleRecord::limitLength));
		return (ok) ? n : -1;
	}
	string getStorageSetId() const {
		string id(storageSetId, sizeof storageSetId);
		eraseTrailingSpaces(id);
		return id;
	}
	bool isValid() const {
		return getSeqNum() != 0 && isValidVersion1() &&
			   string(structure, sizeof structure) == "RECORD" &&
			   getMaxVisRecLen() != -1;
	}
	void read(Input &input_file) {
		input_file.read(this, sizeof *this);
		if (!isValid()) throw RI(RI::BadHdr);
	}
	void write(Output &out) const {
		out.write(this, sizeof *this);
	}
	void reset() 
    {
		intToChars(seqNum, sizeof seqNum, 1);
		memcpy(dlisVersion, "V1.00", sizeof dlisVersion);
		memcpy(structure, "RECORD", sizeof structure);
        // - NB: Вместо memcpy можно было бы использовать (собственную) функцию toChars,
        //		 она медленнее, но с проверкой длины
		intToChars(maxVisRecLen,  sizeof maxVisRecLen, 0);
		memset(storageSetId, ' ', sizeof storageSetId);
	}

	void setStorageSetId(const string &s) 
    {
		if (s.length() > sizeof storageSetId) 
            throw RI(RI::SULLongId);

		toChars(storageSetId, sizeof storageSetId, s, true);
		// - NB: с автоматическим усечением
	}

private:
// НЕПРЕРЫВНАЯ серия полей общей длиной 80 байт
	char        seqNum      [4];
	char        dlisVersion [5];
	char        structure   [6];
	char        maxVisRecLen[5];
	char        storageSetId[60];
// - NB: нужно быть уверенным, что к полям не применяется выравнивание!
}; // class StorageUnitLabel

// ============================================================================

class Reader::Impl : public LogicalFileParent {
public:
private:
	friend class Reader;
	Impl() : m_frMem(false)/*, m_frix(-1)*/ {}
	~Impl() { freeMem(); }
	void freeMem() { clearPtrContainer(mvp_lfs, &LogicalFile::deletePtr); }
	void clear();
	bool loadFramesToMemory() const { return m_frMem; }
	void startRead() 
    {
		m_sul.read(m_in);
		m_ssId = m_sul.getStorageSetId();
	}
	const vector<const LogicalFile *> &cLogicalFiles() const {
		return (vector<const LogicalFile *> &)mvp_lfs;
	}
	void read();
	void loadFrame(const FrameType *frameType, uint32 frameIndex);

	// - Загружает фрейм в режиме m_frMem=false
// Data
	ErrorLogImpl     m_err;
	Input            m_in;
	StorageUnitLabel m_sul;
	string           m_ssId;
	vector<LogicalFile *> mvp_lfs;
	bool             m_frMem;
	// - Признак загрузки фреймов в память в функции read.
	/* - При m_frMem=false в каждом экземпляре Frame запоминается положение
		 сответствующего фрейма во входном DLIS-потоке */
//	uint32 m_frix;	// Индекс текущего загруженного фрейма при m_frMem=false
}; // class Reader::Impl

void Reader::Impl::clear() {
	freeMem();
	m_ssId = "";
	m_sul.reset();
	m_in.close();
	m_err.clear();
	m_frMem = false;
//	m_frix = -1;
}

void Reader::Impl::read() 
{
	if (m_in.atEnd()) 
    {
		m_in.addIssue(RI::NoLFs);
		return;
	}

	bool ended = false;
	while (!ended) 
    {
		LogicalFile *plf = new LogicalFile(this);

		try 
        {
			plf->pim->read(m_in, ended);
            // - Замеренное ускорение выполнения функции Reader::Impl::read (release-сборка)
            //	 при использовании буферизации: - 20% на файле RST.dlis (39 МБайт)
		}
		catch(RI) 
        {
			delete plf; 
            throw;
		}
		mvp_lfs.push_back(plf);
	}
#ifndef NDEBUG
//	for (int nLf = 0; nLf < mvp_lfs.size(); ++nLf) {
//		std::cout << "Logical File #" << nLf + 1 << "\n";
//		LogicalFile *plf = mvp_lfs[nLf];
//		plf->pim->ShowStat(std::cout);
//	}
#endif
}

//void Reader::Impl::loadFrame(const FrameType *frameType, uint32 frameIndex) {
//	if (frameIndex == m_frix)
//		return;	// - нужный фрейм уже загружен
//// Очищаем содержимое ранее загруженного фрейма
//	if (m_frix != -1) frameType->pim->clearSlots(m_frix);
//// Загружаем требуемый фрейм и запоминаем его индекс
//	LogicalFile *plf = LogicalFile::Impl::logicalFileOf(frameType);
//	plf->pim->loadIFLR(m_in, frameType, frameIndex);
//	m_frix = frameIndex;
//}

void Reader::Impl::loadFrame(const FrameType *frameType, uint32 frameIndex) {
	if (frameType->pim->frameLoaded(frameIndex)) return;
	LogicalFile *plf = LogicalFile::Impl::logicalFileOf(frameType);
	plf->pim->loadFrame(m_in, frameType, frameIndex);
}


Reader::Reader() : pim(new Impl) {}

Reader::~Reader() {
	delete pim;
}
// - NB: Если предполагать возможность создания клиентской программой
//		 наследников Reader, то, видимо, нужно сделать деструктор виртуальным

RI Reader::open(std::istream *stream, std::streamsize byteCount) {
	close();
	pim->m_in.open(stream, &pim->m_err, byteCount);
	try {
		pim->startRead();
	}
	catch(RI ri) {
		return ri.toCritical();
	}
	return RI();
}
// - NB: Функция еще не тестировалась (в т.ч. при byteCount!=-1)

RI Reader::open(const string &fileName) 
{

	close();

	try 
    {
	    pim->m_in.open(fileName, &pim->m_err);
	    pim->startRead();
	}
	catch(RI ri)
    {
	    return ri.toCritical();
	}

	return RI();
}

void Reader::close() { pim->clear(); }

//bool Reader::isDlis() const {
//	return pim->m_sul.isValid();
//}

const string &Reader::storageSetId() const { return pim->m_ssId; }
//	{ return pim->m_sul.getStorageSetId(); }

unsigned Reader::logicalFileCount() const { return pim->mvp_lfs.size(); }

Reader::LogicalFileConstIt Reader::cbeginLogicalFile() const
	{ return pim->cLogicalFiles().begin(); }

Reader::LogicalFileConstIt Reader::cendLogicalFile() const
	{ return pim->cLogicalFiles().end(); }

const ErrorLog &Reader::read(bool loadFrames) 
{
	try 
    {
		pim->m_frMem = loadFrames;
		pim->read();
	}
	catch(RI ri) 
    {
		pim->m_in.addIssue(ri.toCritical());
	}
	catch(std::bad_alloc) 
    {
		pim->m_in.addIssue(RI(RI::OutOfMem).toCritical());
	}
	return pim->m_err;
}

RI Reader::readFrame(const FrameType *frameType, uint32 frameIndex) {
	try {
		if (frameIndex >= frameType->frameCount()) throw RI(RI::BadFrameNum, 3);
		if (!pim->m_frMem)	// - режим загрузки фреймов по требованию
			pim->loadFrame(frameType, frameIndex);
		return frameType->pim->fillChannelValues(frameIndex);
	}
	catch(RI ri) {
		return ri.toCritical();
	}
	/* - NB: блок try/catch используется хотя бы потому, что в нем выполняется
			 чтение потока (в виде которого хранится фрейм), размер которого
			 еще не проверен и может оказаться меньшим, чем следует
			 из атрибутов соответствующего Frame-объекта */
}

// ============================================================================

class Writer::Impl : public LogicalFileParent {
public:
private:
	friend class Writer;
	Impl() : m_wCnt(0) {}
	~Impl() {
		flush();
		freeMem();
	}
	void freeMem() { clearPtrContainer(mvp_lfs, &LogicalFile::deletePtr); }
//	const vector<const LogicalFile *> &cLogicalFiles() const
//		{ return (vector<const LogicalFile *> &)mvp_lfs; }
	void clear() {
		flush();
		freeMem();
		m_sul.reset();
		m_out.close();
	}
	void startWrite(const string &id) {
		m_sul.setStorageSetId(id);
		m_ssId = m_sul.getStorageSetId();
		m_sul.write(m_out);
	}
	void flush() {
		if (!mvp_lfs.empty())
			mvp_lfs[mvp_lfs.size() - 1]->pim->finishWrite(m_out);
	}
	LogicalFile *startNewLogicalFile(uint32 seqNum, const ObjectName &defOri) {
		flush();
		LogicalFile *plf = new LogicalFile(this, seqNum, defOri);
		mvp_lfs.push_back(plf);
		return plf;
	}
	RI writeMetadata() {
		if (mvp_lfs.empty()) return RI(RI::NoLFs);
		if (m_wCnt == mvp_lfs.size()) return RI(RI::LFWritten);
		mvp_lfs.back()->pim->writeMetadata(m_out);
		++m_wCnt;
		return RI();
	}
	RI writeFrame(FrameType *frameType) {
		if (mvp_lfs.empty()) return RI(RI::NoLFs);
		if (m_wCnt != mvp_lfs.size()) return RI(RI::LFNotWritten);
		return mvp_lfs.back()->pim->writeFrame(m_out, frameType);
	}
// Data -----------------------------------------------------------------------
//	ErrorLogImpl m_err;
	Output m_out;
	StorageUnitLabel m_sul;
	string m_ssId;
	vector<LogicalFile *> mvp_lfs;
	uint m_wCnt;	// Число записанных логических файлов
}; // class Writer::Impl


Writer::Writer() : pim(new Impl) {}

Writer::~Writer() {
	delete pim;
// - NB: Надо проанализировать предупреждение компилятора на эту инструкцию
}

RI Writer::open(std::ostream *stream, const string &id) {
	try {
		close();
		pim->m_out.open(stream);
		pim->startWrite(id);
	}
	catch(RI ri) {
		return ri.toCritical();
	}
	return RI();
}

RI Writer::open(const string &fileName, const string &id) {
	try {
		close();
		pim->m_out.open(fileName);
		pim->startWrite(id);
	}
	catch(RI ri) {
		return ri.toCritical();
	}
	return RI();
}

void Writer::close() { pim->clear(); }

LogicalFile *Writer::startNewLogicalFile(
		uint32 seqNum, const ObjectName &definingOrigin, RI *retInfo) {
	LogicalFile *plf;
//	if (id.length() > FileHeader::idLength) ri = RI(RI::FHIdTrunc);
	uint32 autoSeqNum = pim->mvp_lfs.empty() ?
						1 : pim->mvp_lfs.back()->seqNum() + 1;
	try {
		if (seqNum < autoSeqNum) {
			if (seqNum != 0) throw RI(RI::FHBadNum);
			seqNum = autoSeqNum;
		}
		if (!definingOrigin.valid()) throw RI(RI::BadObjName, 2);
		if (definingOrigin.originId() > 127) throw RI(RI::BadDefOriId);
//		plf = pim->startNewLogicalFile(id, seqNum);
		plf = pim->startNewLogicalFile(seqNum, definingOrigin);
	}
	catch (RI riGot) {
		if (retInfo) *retInfo = riGot.toCritical();
		return NULL;
	}
	return plf;
}

RI Writer::writeMetadata() {
	try {
		return pim->writeMetadata();
	}
	catch (RI ri) {
		return ri.toCritical();
	}
}

RI Writer::writeNextFrame(FrameType *frameType) {
	try {
		return pim->writeFrame(frameType);
	}
	catch (RI ri) {
		return ri.toCritical();
	}
}

// ============================================================================

#ifndef NDEBUG
// Функции внутреннего отладочного тестирования модуля

bool d_test_IntField() {
	using namespace Representation;
	bool ok = true;
	byte ui8;
	int8 i8;
	int32 i32;
	int16 ui16;

	ok = ok &&  IntTypes::canHold(ui8, 0);
	ok = ok &&  IntTypes::canHold(ui8, 127);
	ok = ok &&  IntTypes::canHold<byte>(128);
	ok = ok &&  IntTypes::canHold<byte>(255);
	ok = ok && !IntTypes::canHold<byte>(256);
	ok = ok && !IntTypes::canHold<byte>(-1);

	ok = ok &&  IntTypes::canHold(i8, 0);
	ok = ok &&  IntTypes::canHold(i8, 127);
	ok = ok &&  IntTypes::canHold<int8>(-128);
	ok = ok && !IntTypes::canHold<int8>(128);
	ok = ok && !IntTypes::canHold<int8>(-129);
	ok = ok && !IntTypes::canHold<int8>(255);
	ok = ok && !IntTypes::canHold<int8>(-256);

	ok = ok &&  IntTypes::canHold<int16, int8>();
	ok = ok && !IntTypes::canHold<int8, byte>();

	ok = ok && !DlisIntTypes::canHoldCode(i32, ULONG);
	ok = ok &&  DlisIntTypes::canHoldCode(i32, SLONG);
	ok = ok && !DlisIntTypes::canHoldCode(i8, SNORM);

	ok = ok &&  DlisIntTypes::canHoldType(ULONG, ui8);
	ok = ok && !DlisIntTypes::canHoldType(ULONG, i32);

	ok = ok &&  DlisIntTypes::canHoldValue(USHORT, 255);
	ok = ok && !DlisIntTypes::canHoldValue(USHORT, -1);
	ok = ok && !DlisIntTypes::canHoldValue(USHORT, 256);

	ok = ok && !DlisIntTypes::canHoldValue(SSHORT, 128);
	ok = ok && !DlisIntTypes::canHoldValue(ULONG, -1);

	return ok;
}

bool d_test_ValueConvertor() {
	using namespace Representation;
	RI riSet, riGet;
	bool ok, okAll = true;
	SingleValue v;
	int32 iSet, iGet;

	iSet = 2;
	riSet = v.set(&iSet, 1, ULONG);
	riGet = v.get(&iGet, 0, 1);
	ok = riSet.ok() && riGet.ok() && iGet == iSet;
	okAll = okAll && ok;

	iSet = Representation::maxUVARI;
	riSet = v.set(&iSet, 1, UVARI);
	riGet = v.get(&iGet, 0, 1);
	ok = riSet.ok() && riGet.ok() && iGet == iSet;
	okAll = okAll && ok;

	uint32 uiSet, uiGet;
	uiSet = Representation::maxUVARI + 1;
	riSet = v.set(&uiSet, 1, ULONG);
	riGet = v.get(&uiGet, 0, 1);
	ok = riSet.ok() && riGet.ok() && uiGet == uiSet;
	okAll = okAll && ok;


	return okAll;
}

#endif // NDEBUG

// ============================================================================

// ===== Схема реализации PIMPL-идиомы для древовидной структуры =====
namespace Pimpl_Sample {

class Parent;
class Child;

// Интерфейс (h-файл)

class T {
public:
	~T();
private:
	friend class Parent;
	T();
	class Impl;
	Impl *pim;
// - NB: auto_ptr не используем хотя бы потому, что в C++11
//		 он считается deprecated
};

// Реализация (cpp-файл)

class T::Impl {
public:
	Impl() {}
	~Impl() {
		clearPtrContainer(m_children);
	}
private:
	friend class T;
	vector <Child *> m_children;
};

T::T() :
	pim(new Impl()) {}

T::~T() {
	delete pim;
}

} // namespace Pimpl_Frame

}; // namespace Dlis

#pragma warning(pop)
