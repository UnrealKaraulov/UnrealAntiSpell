#ifndef INIWRITER_H
#define INIWRITER_H
#include <string>



class CIniWriter
{
public:
	CIniWriter(std::string szFileName);
	CIniWriter();
	void WriteInteger(char* szSection, char* szKey, int iValue);
	void WriteFloat(char* szSection, char* szKey, float fltValue);
	void WriteBOOLean(char* szSection, char* szKey, BOOL bolValue);
	void WriteString(char* szSection, char* szKey, const char* szValue);
private:
	char m_szFileName[255];
};
#endif //INIWRITER_H